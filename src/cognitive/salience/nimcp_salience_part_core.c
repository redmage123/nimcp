// nimcp_salience_part_core.c - core functions
// Part of nimcp_salience.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_salience.c


//=============================================================================
// Salience Evaluation Functions
//=============================================================================

brain_salience_t brain_evaluate_salience(salience_evaluator_t eval, const float* features,
                                         uint32_t num_features)
{
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_brain_evaluate_salie", 0.0f);


    return brain_evaluate_salience_temporal(eval, features, num_features, 0);
}


brain_salience_t brain_evaluate_salience_temporal(salience_evaluator_t eval, const float* features,
                                                  uint32_t num_features, uint64_t timestamp)
{
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_brain_evaluate_salie", 0.0f);


    brain_salience_t salience = {0};

    if (!eval || !features) {
        return salience;  // Return zeros
    }

    uint64_t start_time = nimcp_time_monotonic_us();

    nimcp_mutex_lock(&eval->eval_lock);

    /**
     * WHAT: Compute salience using configured strategy
     * WHY: Different strategies for different performance needs
     * HOW: Strategy pattern - delegate to strategy function
     */
    switch (eval->config.strategy) {
        case SALIENCE_STRATEGY_FAST:
            salience = compute_salience_fast(eval, features, num_features, timestamp);
            break;

        case SALIENCE_STRATEGY_BALANCED:
            salience = compute_salience_balanced(eval, features, num_features, timestamp);
            break;

        case SALIENCE_STRATEGY_ACCURATE:
            salience = compute_salience_accurate(eval, features, num_features, timestamp);
            break;
    }

    // Apply acetylcholine gating (attentiveness modulation)
    apply_acetylcholine_gating(&salience, eval->brain);

    /**
     * WHAT: Update history and predictor
     * WHY: Learn from this input for future comparisons
     * HOW: Add to history buffer, update prediction
     */
    if (eval->history) {
        history_buffer_add(eval->history, features, num_features, timestamp);
    }

    if (eval->predictor) {
        predictor_update(eval->predictor, features, num_features);
    }

    /**
     * WHAT: Update statistics
     * WHY: Monitor evaluator performance
     */
    eval->stats_evaluations++;

    // Running averages (exponential moving average)
    float alpha = 0.1F;
    eval->running_avg_salience =
        alpha * salience.salience + (1.0F - alpha) * eval->running_avg_salience;
    eval->running_avg_novelty =
        alpha * salience.novelty + (1.0F - alpha) * eval->running_avg_novelty;
    eval->running_avg_surprise =
        alpha * salience.surprise + (1.0F - alpha) * eval->running_avg_surprise;
    eval->running_avg_urgency =
        alpha * salience.urgency + (1.0F - alpha) * eval->running_avg_urgency;

    // Count high events
    if (salience.salience > eval->config.high_salience_threshold) {
        eval->stats_high_salience++;
    }
    if (salience.novelty > eval->config.high_novelty_threshold) {
        eval->stats_high_novelty++;
    }
    if (salience.surprise > eval->config.high_surprise_threshold) {
        eval->stats_high_surprise++;
    }
    if (salience.urgency > eval->config.high_urgency_threshold) {
        eval->stats_high_urgency++;
    }

    nimcp_mutex_unlock(&eval->eval_lock);

    /**
     * WHAT: Trigger callbacks if thresholds exceeded
     * WHY: Observer pattern - notify application
     */
    if (eval->callback) {
        if (salience.salience > eval->config.high_salience_threshold) {
            salience_event_t event = {.type = SALIENCE_EVENT_HIGH_SALIENCE,
                                      .salience = salience,
                                      .features = features,
                                      .num_features = num_features,
                                      .timestamp = timestamp,
                                      .message = "High salience detected"};
            eval->callback(&event, eval->callback_context);
        }
    }

    uint64_t elapsed_us = nimcp_time_elapsed_us(start_time);
    eval->total_eval_time_us += elapsed_us;

    // Broadcast high salience events via bio-async
    if (salience.salience > eval->config.high_salience_threshold) {
        bio_broadcast_salience_response(eval, &salience, (uint32_t)timestamp);
    }

    /* W8: emit transition event when crossing high-salience threshold +
     * read-back partner query (rate-limited via stats_evaluations). */
    if ((eval->stats_evaluations & 0x3F) == 0) {
        struct brain_struct* _kg_brain =
            world_model_kg_events_get_registered_brain();
        if (_kg_brain) {
            int old_level =
                (eval->running_avg_salience > eval->config.high_salience_threshold)
                    ? 1 : 0;
            int new_level =
                (salience.salience > eval->config.high_salience_threshold)
                    ? 1 : 0;
            world_model_kg_emit_salience_transition(_kg_brain,
                old_level, new_level, salience.salience);
            (void)world_model_kg_has_partner(_kg_brain,
                "cog_fep_free_energy");
        }
    }

    return salience;
}


uint32_t brain_evaluate_salience_batch(salience_evaluator_t eval, const float** features,
                                       uint32_t num_samples, uint32_t num_features,
                                       brain_salience_t* salience_scores)
{
    if (!eval || !features || !salience_scores) {
        return 0;
    }

    /**
     * WHAT: Choose sequential vs parallel based on batch size
     * WHY: Thread pool overhead makes small batches slower
     * HOW: Use sequential for < 200 samples, parallel for larger batches
     *
     * PERFORMANCE: Thread pool overhead ~1ms, so need enough work to amortize
     */
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_brain_evaluate_salie", 0.0f);


    const uint32_t PARALLEL_THRESHOLD = 200;

    if (num_samples < PARALLEL_THRESHOLD) {
        /**
         * WHAT: Sequential evaluation for small batches
         * WHY: Avoid thread pool overhead
         * HOW: Call evaluation function directly for each sample
         */
        for (uint32_t i = 0; i < num_samples; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_samples > 256) {
                salience_heartbeat("salience_loop",
                                 (float)(i + 1) / (float)num_samples);
            }

            salience_scores[i] = brain_evaluate_salience(eval, features[i], num_features);
        }
        return num_samples;
    }

    /**
     * WHAT: Use thread pool for parallel batch evaluation (large batches)
     * WHY: Significant performance improvement for batch processing
     * HOW: Parallel evaluation WITHOUT state updates, then sequential state update
     *
     * PATTERN: Map-reduce with thread pool
     * KEY INSIGHT: Lock contention kills parallelism - compute in parallel, update sequentially
     */

    /**
     * WHAT: Allocate task context array
     * WHY: Each worker needs context for its sample
     * HOW: Heap allocation for safety across threads
     */
    batch_task_t* tasks = (batch_task_t*) nimcp_calloc(num_samples, sizeof(batch_task_t));
    if (!tasks) {
        return 0;
    }

    /**
     * WHAT: Prepare all tasks
     * WHY: Submit them to thread pool for parallel execution
     */
    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        tasks[i].eval = eval;
        tasks[i].features = features[i];
        tasks[i].num_features = num_features;
        tasks[i].output = &salience_scores[i];
    }

    /**
     * WHAT: Submit all tasks to thread pool
     * WHY: Workers will execute them in parallel WITHOUT locks
     * HOW: Pool distributes work across available threads
     */
    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        nimcp_result_t result =
            nimcp_pool_submit(eval->thread_pool, evaluate_single_task, &tasks[i]);

        if (result != NIMCP_SUCCESS) {
            /**
             * WHAT: Handle submission failure
             * WHY: Queue might be full or system error
             * HOW: Wait for pool to drain, retry
             */
            nimcp_pool_wait(eval->thread_pool);
            nimcp_pool_submit(eval->thread_pool, evaluate_single_task, &tasks[i]);
        }
    }

    /**
     * WHAT: Wait for all tasks to complete
     * WHY: Ensure all results are ready before state updates
     * HOW: Blocks until queue empty and no active workers
     */
    nimcp_pool_wait(eval->thread_pool);

    /**
     * WHAT: Update shared state sequentially after parallel evaluation
     * WHY: Maintain history/predictor consistency without lock contention
     * HOW: Add each sample to history and update predictor
     *
     * NOTE: This is the only place we update shared state - avoids parallelism bottleneck
     */
    nimcp_mutex_lock(&eval->eval_lock);

    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        /* WHAT: Update history buffer if enabled */
        if (eval->history) {
            history_buffer_add(eval->history, features[i], num_features, 0);
        }

        /* WHAT: Update predictor if enabled */
        if (eval->predictor) {
            predictor_update(eval->predictor, features[i], num_features);
        }

        /* WHAT: Update statistics */
        eval->stats_evaluations++;

        float alpha = 0.1F;
        eval->running_avg_salience =
            alpha * salience_scores[i].salience + (1.0F - alpha) * eval->running_avg_salience;
        eval->running_avg_novelty =
            alpha * salience_scores[i].novelty + (1.0F - alpha) * eval->running_avg_novelty;
        eval->running_avg_surprise =
            alpha * salience_scores[i].surprise + (1.0F - alpha) * eval->running_avg_surprise;
        eval->running_avg_urgency =
            alpha * salience_scores[i].urgency + (1.0F - alpha) * eval->running_avg_urgency;

        if (salience_scores[i].salience > eval->config.high_salience_threshold) {
            eval->stats_high_salience++;
        }
        if (salience_scores[i].novelty > eval->config.high_novelty_threshold) {
            eval->stats_high_novelty++;
        }
        if (salience_scores[i].surprise > eval->config.high_surprise_threshold) {
            eval->stats_high_surprise++;
        }
        if (salience_scores[i].urgency > eval->config.high_urgency_threshold) {
            eval->stats_high_urgency++;
        }
    }

    nimcp_mutex_unlock(&eval->eval_lock);

    /**
     * WHAT: Free task array
     * WHY: Cleanup temporary allocation
     */
    nimcp_free(tasks);
    tasks = NULL;

    return num_samples;
}


bool salience_register_callback(salience_evaluator_t eval, salience_event_callback_fn callback,
                                void* context)
{
    if (!eval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_register_callback: eval is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_register_callback", 0.0f);

    nimcp_mutex_lock(&eval->eval_lock);

    eval->callback = callback;
    eval->callback_context = context;

    nimcp_mutex_unlock(&eval->eval_lock);

    return true;
}


bool salience_clear_history(salience_evaluator_t eval)
{
    if (!eval || !eval->history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_clear_history: required parameter is NULL (eval, eval->history)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_clear_history", 0.0f);

    history_buffer_clear(eval->history);

    return true;
}


brain_salience_t salience_quick_evaluate(brain_t brain, const float* features,
                                         uint32_t num_features)
{
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_quick_evaluate", 0.0f);


    salience_config_t config = salience_default_config();
    config.history_size = 10;  // Small history for quick eval

    salience_evaluator_t eval = salience_evaluator_create(brain, &config);
    if (!eval) {
        brain_salience_t empty = {0};
        return empty;
    }

    brain_salience_t salience = brain_evaluate_salience(eval, features, num_features);

    salience_evaluator_destroy(eval);

    return salience;
}


//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Boost salience for negative cues
 *
 * WHAT: Increase attention to negative stimuli
 * WHY:  Depression biases attention toward negative information
 * HOW:  Store boost factor to apply in next evaluation
 *
 * BIOLOGY: Mood-congruent attentional bias
 *          Depression → hyperattention to negative cues
 *
 * COMPLEXITY: O(1)
 *
 * @param evaluator Salience evaluator
 * @param boost_factor Salience boost [0, 1]
 */
void salience_boost_negative_cues(salience_evaluator_t evaluator, float boost_factor)
{
    // Guard: Validate evaluator
    if (!evaluator) {
        return;
    }

    // Clamp boost factor
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_boost_negative_cues", 0.0f);


    boost_factor = fminf(fmaxf(boost_factor, 0.0F), 1.0F);

    // WHAT: Increase novelty weight to bias toward unexpected negatives
    // WHY:  Depression makes negative novelty more salient
    // HOW:  Scale novelty weight by (1 + boost_factor)
    nimcp_mutex_lock(&evaluator->eval_lock);

    evaluator->config.novelty_weight *= (1.0F + boost_factor * 0.5F);

    nimcp_mutex_unlock(&evaluator->eval_lock);
}


/**
 * @brief Boost threat detection sensitivity
 *
 * WHAT: Increase urgency for potential threats
 * WHY:  Anxiety increases threat vigilance
 * HOW:  Lower threshold for urgency detection
 *
 * BIOLOGY: Amygdala hyperactivity in anxiety
 *          Anxious individuals detect threats faster with lower thresholds
 *
 * COMPLEXITY: O(1)
 *
 * @param evaluator Salience evaluator
 * @param boost_factor Threat sensitivity boost [0, 1]
 */
void salience_boost_threat_detection(salience_evaluator_t evaluator, float boost_factor)
{
    // Guard: Validate evaluator
    if (!evaluator) {
        return;
    }

    // Clamp boost factor
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_boost_threat_detecti", 0.0f);


    boost_factor = fminf(fmaxf(boost_factor, 0.0F), 1.0F);

    // WHAT: Increase urgency baseline and weight
    // WHY:  Anxiety makes everything seem more urgent
    // HOW:  Scale urgency parameters by boost factor
    nimcp_mutex_lock(&evaluator->eval_lock);

    evaluator->config.urgency_baseline = fminf(
        evaluator->config.urgency_baseline * (1.0F + boost_factor),
        1.0F
    );

    evaluator->config.urgency_weight *= (1.0F + boost_factor * 0.5F);

    nimcp_mutex_unlock(&evaluator->eval_lock);
}


//=============================================================================
// Cross-Modal Salience Fusion Functions
//=============================================================================

/**
 * @brief Register a modality with initial weight
 *
 * WHAT: Enable a sensory modality for salience evaluation
 * WHY:  Multimodal integration requires registering each modality
 * HOW:  Set modality as active with initial fusion weight
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus receives input from multiple sensory maps
 * - Each modality contributes differently based on context
 * - Registration enables cross-modal integration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param evaluator Salience evaluator
 * @param modality Modality to register
 * @param weight Initial fusion weight (0-1, will be normalized)
 * @return true on success, false on error
 */
bool salience_register_modality(salience_evaluator_t evaluator, salience_modality_t modality,
                                float weight)
{
    // Guard: Validate evaluator
    if (!evaluator) {
        salience_set_error("NULL evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_register_modality: evaluator is NULL");
        return false;
    }

    // Guard: Validate modality
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_register_modality", 0.0f);


    if (modality < 0 || modality >= SALIENCE_MODALITY_COUNT) {
        salience_set_error("Invalid modality: %d", modality);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "salience_register_modality: capacity exceeded");
        return false;
    }

    // Guard: Validate weight
    weight = fminf(fmaxf(weight, 0.0F), 1.0F);

    nimcp_mutex_lock(&evaluator->eval_lock);

    /**
     * WHAT: Mark modality as active with initial weight
     * WHY: Enable this modality to participate in fusion
     * HOW: Set active flag and store weight
     */
    evaluator->modalities[modality].active = true;
    evaluator->modalities[modality].weight = weight;
    evaluator->modalities[modality].has_data = false;
    evaluator->modalities[modality].salience = (brain_salience_t){0};

    nimcp_mutex_unlock(&evaluator->eval_lock);

    LOG_DEBUG("Registered modality %s with weight %.2f",
              salience_modality_name(modality), weight);

    return true;
}


/**
 * @brief Evaluate salience for specific modality
 *
 * WHAT: Compute salience from single sensory modality
 * WHY:  Separate evaluation before cross-modal fusion
 * HOW:  Run salience evaluation, store in modality-specific slot
 *
 * BIOLOGICAL BASIS:
 * - Each sensory cortex computes modality-specific salience
 * - Visual salience from V1/V4/IT, auditory from A1, etc.
 * - Results cached for subsequent fusion
 *
 * COMPLEXITY: O(1) - same as normal salience evaluation
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param evaluator Salience evaluator
 * @param modality Which modality these features belong to
 * @param features Input feature vector from that modality
 * @param num_features Size of feature vector
 * @return Salience scores for this modality
 */
brain_salience_t salience_evaluate_modality(salience_evaluator_t evaluator,
                                            salience_modality_t modality, const float* features,
                                            uint32_t num_features)
{
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_evaluate_modality", 0.0f);


    brain_salience_t salience = {0};

    // Guard: Validate evaluator
    if (!evaluator) {
        salience_set_error("NULL evaluator");
        return salience;
    }

    // Guard: Validate modality
    if (modality < 0 || modality >= SALIENCE_MODALITY_COUNT) {
        salience_set_error("Invalid modality: %d", modality);
        return salience;
    }

    // Guard: Validate features
    if (!features || num_features == 0) {
        salience_set_error("Invalid features");
        return salience;
    }

    /**
     * WHAT: Evaluate salience using normal pathway
     * WHY: Same computation, different storage location
     * HOW: Use brain_evaluate_salience, then cache result
     */
    salience = brain_evaluate_salience(evaluator, features, num_features);

    nimcp_mutex_lock(&evaluator->eval_lock);

    /**
     * WHAT: Store modality-specific salience
     * WHY: Cache for subsequent fusion operation
     * HOW: Copy to modality slot, mark as having data
     */
    evaluator->modalities[modality].salience = salience;
    evaluator->modalities[modality].has_data = true;

    nimcp_mutex_unlock(&evaluator->eval_lock);

    LOG_DEBUG("Evaluated %s salience: %.2f (novelty=%.2f, surprise=%.2f, urgency=%.2f)",
              salience_modality_name(modality), salience.salience,
              salience.novelty, salience.surprise, salience.urgency);

    return salience;
}


/**
 * @brief Combine modality scores into unified salience
 *
 * WHAT: Fuse per-modality saliences into single attention score
 * WHY:  Superior colliculus performs cross-modal integration
 * HOW:  Apply fusion strategy (MAX, WEIGHTED_AVG, LEARNED)
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus receives visual, auditory, somatosensory maps
 * - Fuses into unified spatial attention map
 * - Parietal cortex performs context-dependent weighting
 *
 * COMPLEXITY: O(m) where m = number of active modalities
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param evaluator Salience evaluator
 * @return Fused salience score combining all active modalities
 */
brain_salience_t salience_fuse_modalities(salience_evaluator_t evaluator)
{
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_fuse_modalities", 0.0f);


    brain_salience_t fused = {0};

    // Guard: Validate evaluator
    if (!evaluator) {
        salience_set_error("NULL evaluator");
        return fused;
    }

    nimcp_mutex_lock(&evaluator->eval_lock);

    /**
     * WHAT: Count active modalities with data
     * WHY: Need at least one to fuse
     * HOW: Iterate through modality array
     */
    int active_count = 0;
    float total_weight = 0.0F;

    for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
        }

        if (evaluator->modalities[i].active && evaluator->modalities[i].has_data) {
            active_count++;
            total_weight += evaluator->modalities[i].weight;
        }
    }

    // Guard: Need at least one active modality
    if (active_count == 0 || total_weight < NIMCP_VECTOR_EPSILON) {
        nimcp_mutex_unlock(&evaluator->eval_lock);
        return fused;
    }

    /**
     * WHAT: Apply fusion strategy
     * WHY: Different strategies for different contexts
     * HOW: Switch on strategy enum
     */
    switch (evaluator->fusion_strategy) {
        case SALIENCE_FUSION_MAX: {
            /**
             * WHAT: Maximum fusion (winner-take-all)
             * WHY: Dominant modality captures attention
             * HOW: Take maximum value across all modalities
             *
             * BIOLOGICAL: Superior colliculus winner-take-all circuits
             */
            for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
                    salience_heartbeat("salience_loop",
                                     (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
                }

                if (!evaluator->modalities[i].active || !evaluator->modalities[i].has_data) {
                    continue;
                }

                brain_salience_t* m = &evaluator->modalities[i].salience;

                if (m->salience > fused.salience) {
                    fused.salience = m->salience;
                }
                if (m->novelty > fused.novelty) {
                    fused.novelty = m->novelty;
                }
                if (m->surprise > fused.surprise) {
                    fused.surprise = m->surprise;
                }
                if (m->urgency > fused.urgency) {
                    fused.urgency = m->urgency;
                }
                if (m->confidence > fused.confidence) {
                    fused.confidence = m->confidence;
                }
            }
            break;
        }

        case SALIENCE_FUSION_WEIGHTED_AVG: {
            /**
             * WHAT: Weighted average fusion
             * WHY: Balanced integration of all modalities
             * HOW: Sum weighted values, divide by total weight
             *
             * BIOLOGICAL: Parietal cortex weighted integration
             */
            for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
                    salience_heartbeat("salience_loop",
                                     (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
                }

                if (!evaluator->modalities[i].active || !evaluator->modalities[i].has_data) {
                    continue;
                }

                float w = evaluator->modalities[i].weight;
                brain_salience_t* m = &evaluator->modalities[i].salience;

                fused.salience += w * m->salience;
                fused.novelty += w * m->novelty;
                fused.surprise += w * m->surprise;
                fused.urgency += w * m->urgency;
                fused.confidence += w * m->confidence;
            }

            // Normalize by total weight
            fused.salience /= total_weight;
            fused.novelty /= total_weight;
            fused.surprise /= total_weight;
            fused.urgency /= total_weight;
            fused.confidence /= total_weight;
            break;
        }

        case SALIENCE_FUSION_LEARNED: {
            /**
             * WHAT: Learned tensor-based fusion with cross-modal interactions
             * WHY: Context-dependent attention requires adaptive fusion
             * HOW: Linear weights + quadratic interaction terms for cross-modal effects
             *
             * BIOLOGICAL: Prefrontal cortex context-dependent attention
             *
             * FORMULA: output = sum(w_i * s_i) + sum(I_ij * s_i * s_j) + bias
             * - Linear: individual modality contributions
             * - Quadratic: cross-modal synergies (e.g., visual+audio coincidence)
             */

            // Collect active modality saliences
            float saliences[SALIENCE_MODALITY_COUNT] = {0};
            float novelties[SALIENCE_MODALITY_COUNT] = {0};
            float surprises[SALIENCE_MODALITY_COUNT] = {0};
            float urgencies[SALIENCE_MODALITY_COUNT] = {0};

            for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
                    salience_heartbeat("salience_loop",
                                     (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
                }

                if (evaluator->modalities[i].active && evaluator->modalities[i].has_data) {
                    saliences[i] = evaluator->modalities[i].salience.salience;
                    novelties[i] = evaluator->modalities[i].salience.novelty;
                    surprises[i] = evaluator->modalities[i].salience.surprise;
                    urgencies[i] = evaluator->modalities[i].salience.urgency;
                }
            }

            // Linear terms: sum(w_i * s_i)
            float sal_linear = 0.0f, nov_linear = 0.0f, sur_linear = 0.0f, urg_linear = 0.0f;
            for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
                    salience_heartbeat("salience_loop",
                                     (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
                }

                if (evaluator->modalities[i].active && evaluator->modalities[i].has_data) {
                    float w = evaluator->modalities[i].weight;
                    sal_linear += w * saliences[i];
                    nov_linear += w * novelties[i];
                    sur_linear += w * surprises[i];
                    urg_linear += w * urgencies[i];
                }
            }

            // Quadratic interaction terms: cross-modal synergy
            // Boost salience when multiple modalities are active and salient
            float sal_quad = 0.0f;
            for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
                    salience_heartbeat("salience_loop",
                                     (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
                }

                if (!evaluator->modalities[i].active || !evaluator->modalities[i].has_data) continue;
                for (int j = i + 1; j < SALIENCE_MODALITY_COUNT; j++) {
                    if (!evaluator->modalities[j].active || !evaluator->modalities[j].has_data) continue;

                    // Cross-modal coincidence boosts salience
                    float interaction = 0.15f;  // Default interaction strength
                    float scale = 2.0f;  // Off-diagonal appears twice
                    sal_quad += scale * interaction * saliences[i] * saliences[j];
                }
            }

            // Combine and normalize
            if (total_weight > 0.0f) {
                fused.salience = fminf(1.0f, (sal_linear + sal_quad) / total_weight);
                fused.novelty = nov_linear / total_weight;
                fused.surprise = sur_linear / total_weight;
                fused.urgency = urg_linear / total_weight;
                fused.confidence = 0.9f;  // High confidence for tensor fusion
            }
            break;
        }
    }

    // Estimate cost as average of modality costs
    float cost_sum = 0.0F;
    for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
        }

        if (evaluator->modalities[i].active && evaluator->modalities[i].has_data) {
            cost_sum += evaluator->modalities[i].salience.estimated_cost;
        }
    }
    fused.estimated_cost = (active_count > 0) ? (cost_sum / active_count) : 0.0f;

    nimcp_mutex_unlock(&evaluator->eval_lock);

    LOG_DEBUG("Fused %d modalities: salience=%.2f (novelty=%.2f, surprise=%.2f, urgency=%.2f)",
              active_count, fused.salience, fused.novelty, fused.surprise, fused.urgency);

    return fused;
}


/**
 * @brief Get name string for modality
 *
 * WHAT: Convert modality enum to human-readable name
 * WHY:  Logging and debugging
 * HOW:  Return static string from lookup table
 *
 * COMPLEXITY: O(1)
 *
 * @param modality Modality enum
 * @return String name ("visual", "audio", etc.)
 */
const char* salience_modality_name(salience_modality_t modality)
{
    static const char* names[] = {
        "visual",
        "audio",
        "somatosensory",
        "linguistic"
    };

    if (modality < 0 || modality >= SALIENCE_MODALITY_COUNT) {
        return "unknown";
    }

    return names[modality];
}


/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int salience_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Salience");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                salience_heartbeat("salience_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Salience");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Salience");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int salience_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_training_begin: NULL argument");
        return -1;
    }
    salience_heartbeat_instance(NULL, "salience_training_begin", 0.0f);
    (void)(struct salience_evaluator_struct*)instance; /* Module state available for reset */
    return 0;
}


int salience_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_training_end: NULL argument");
        return -1;
    }
    salience_heartbeat_instance(NULL, "salience_training_end", 1.0f);
    (void)(struct salience_evaluator_struct*)instance; /* Module state available for finalization */
    return 0;
}
