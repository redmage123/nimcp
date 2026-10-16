// nimcp_salience_part_accessors.c - accessors functions
// Part of nimcp_salience.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_salience.c


static void salience_set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_salience_error, sizeof(g_salience_error), format, args);
    va_end(args);
}


const char* salience_get_last_error(void)
{
    return g_salience_error;
}


//=============================================================================
// Salience Evaluator Lifecycle (Factory Pattern)
//=============================================================================

/**
 * WHAT: Validate salience configuration
 * WHY: Guard clause pattern - fail fast on invalid config
 */
static bool validate_salience_config(const salience_config_t* config)
{
    if (!config) {
        salience_set_error("NULL configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_salience_config: config is NULL");
        return false;
    }

    if (config->history_size == 0 && config->enable_novelty) {
        salience_set_error("Novelty requires non-zero history size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_salience_config: config->history_size is zero");
        return false;
    }

    if (config->history_size > 10000) {
        salience_set_error("History size too large (max 10000)");
        return false;
    }

    return true;
}


//=============================================================================
// Configuration and Control Functions
//=============================================================================

bool salience_set_weights(salience_evaluator_t eval, float novelty_weight, float surprise_weight,
                          float urgency_weight)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_set_weights", 0.0f);


    if (eval && eval->bio_ctx) {
        bio_router_process_inbox(eval->bio_ctx, 5);
    }

    if (!eval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_set_weights: eval is NULL");
        return false;
    }

    nimcp_mutex_lock(&eval->eval_lock);

    eval->config.novelty_weight = novelty_weight;
    eval->config.surprise_weight = surprise_weight;
    eval->config.urgency_weight = urgency_weight;

    nimcp_mutex_unlock(&eval->eval_lock);

    return true;
}


bool salience_set_thresholds(salience_evaluator_t eval, float high_salience_threshold,
                             float high_novelty_threshold, float high_surprise_threshold,
                             float high_urgency_threshold)
{
    if (!eval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_set_thresholds: eval is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_set_thresholds", 0.0f);

    nimcp_mutex_lock(&eval->eval_lock);

    eval->config.high_salience_threshold = high_salience_threshold;
    eval->config.high_novelty_threshold = high_novelty_threshold;
    eval->config.high_surprise_threshold = high_surprise_threshold;
    eval->config.high_urgency_threshold = high_urgency_threshold;

    nimcp_mutex_unlock(&eval->eval_lock);

    return true;
}


bool salience_get_stats(salience_evaluator_t eval, salience_stats_t* stats)
{
    if (!eval || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_get_stats: required parameter is NULL (eval, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_get_stats", 0.0f);

    nimcp_mutex_lock(&eval->eval_lock);

    stats->evaluations_performed = eval->stats_evaluations;
    stats->high_salience_count = eval->stats_high_salience;
    stats->high_novelty_count = eval->stats_high_novelty;
    stats->high_surprise_count = eval->stats_high_surprise;
    stats->high_urgency_count = eval->stats_high_urgency;

    stats->avg_salience = eval->running_avg_salience;
    stats->avg_novelty = eval->running_avg_novelty;
    stats->avg_surprise = eval->running_avg_surprise;
    stats->avg_urgency = eval->running_avg_urgency;

    stats->avg_evaluation_time_us = eval->stats_evaluations > 0
                                        ? (float) eval->total_eval_time_us / eval->stats_evaluations
                                        : 0.0F;

    stats->history_size = eval->history ? eval->history->count : 0;

    // Cache hit rate from evaluator's internal cache stats
    // In this implementation, caching is via history buffer reuse
    // Compute "cache" hit rate as ratio of fast-path evaluations
    uint64_t total_evals = eval->stats_evaluations;
    uint64_t high_sal = eval->stats_high_salience;
    if (total_evals > 0) {
        // Fast evaluations = those that didn't trigger high salience
        // (since high salience triggers deeper evaluation)
        float fast_ratio = (float)(total_evals - high_sal) / (float)total_evals;
        stats->cache_hit_rate = (uint32_t)(fast_ratio * 100.0f);
    } else {
        stats->cache_hit_rate = 0;
    }

    nimcp_mutex_unlock(&eval->eval_lock);

    return true;
}


//=============================================================================
// Convenience Functions
//=============================================================================

salience_config_t salience_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_default_config", 0.0f);


    salience_config_t config = {.strategy = SALIENCE_STRATEGY_BALANCED,
                                .history_size = 100,
                                .enable_novelty = true,
                                .enable_surprise = true,
                                .enable_urgency = true,
                                .enable_prediction = true,
                                .urgency_baseline = 0.3F,
                                .novelty_weight = 0.3F,
                                .surprise_weight = 0.4F,
                                .urgency_weight = 0.3F,
                                .high_salience_threshold = 0.7F,
                                .high_novelty_threshold = 0.8F,
                                .high_surprise_threshold = 0.8F,
                                .high_urgency_threshold = 0.9F,
                                .enable_caching = false,
                                .cache_size = 0};

    return config;
}


/**
 * @brief Get surprise level from recent evaluation
 *
 * WHAT: Query most recent surprise score
 * WHY:  Emotional system modulates arousal based on surprise
 * HOW:  Return average surprise from running statistics
 *
 * COMPLEXITY: O(1)
 *
 * @param evaluator Salience evaluator
 * @return Surprise level [0, 1]
 */
float salience_get_surprise_level(salience_evaluator_t evaluator)
{
    // Guard: Validate evaluator
    if (!evaluator) {
        return 0.0F;
    }

    // WHAT: Return running average surprise
    // WHY:  More stable than single sample
    // HOW:  Read from statistics
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_get_surprise_level", 0.0f);


    nimcp_mutex_lock(&evaluator->eval_lock);

    float surprise = evaluator->running_avg_surprise;

    nimcp_mutex_unlock(&evaluator->eval_lock);

    return surprise;
}


/**
 * @brief Adjust fusion weight for modality
 *
 * WHAT: Change how much a modality contributes to attention
 * WHY:  Context-dependent attention weighting (e.g., focus on audio in dark)
 * HOW:  Update modality weight, will be normalized with others
 *
 * BIOLOGICAL BASIS:
 * - Attention can bias toward specific modalities
 * - Top-down control from prefrontal cortex
 * - "Listen carefully" increases auditory weight
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param evaluator Salience evaluator
 * @param modality Modality to adjust
 * @param weight New weight (0-1, will be normalized)
 * @return true on success
 */
bool salience_set_modality_weight(salience_evaluator_t evaluator, salience_modality_t modality,
                                  float weight)
{
    // Guard: Validate evaluator
    if (!evaluator) {
        salience_set_error("NULL evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_set_modality_weight: evaluator is NULL");
        return false;
    }

    // Guard: Validate modality
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_set_modality_weight", 0.0f);


    if (modality < 0 || modality >= SALIENCE_MODALITY_COUNT) {
        salience_set_error("Invalid modality: %d", modality);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "salience_set_modality_weight: capacity exceeded");
        return false;
    }

    // Clamp weight
    weight = fminf(fmaxf(weight, 0.0F), 1.0F);

    nimcp_mutex_lock(&evaluator->eval_lock);

    evaluator->modalities[modality].weight = weight;

    nimcp_mutex_unlock(&evaluator->eval_lock);

    LOG_DEBUG("Set %s weight to %.2f", salience_modality_name(modality), weight);

    return true;
}


/**
 * @brief Get per-modality salience scores
 *
 * WHAT: Query salience for specific modality
 * WHY:  Inspect which modality is driving attention
 * HOW:  Return cached modality-specific salience
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param evaluator Salience evaluator
 * @param modality Which modality to query
 * @return Salience scores for that modality (zeros if not active)
 */
brain_salience_t salience_get_modality_salience(salience_evaluator_t evaluator,
                                                salience_modality_t modality)
{
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_get_modality_salienc", 0.0f);


    brain_salience_t salience = {0};

    // Guard: Validate evaluator
    if (!evaluator) {
        return salience;
    }

    // Guard: Validate modality
    if (modality < 0 || modality >= SALIENCE_MODALITY_COUNT) {
        return salience;
    }

    nimcp_mutex_lock(&evaluator->eval_lock);

    if (evaluator->modalities[modality].active && evaluator->modalities[modality].has_data) {
        salience = evaluator->modalities[modality].salience;
    }

    nimcp_mutex_unlock(&evaluator->eval_lock);

    return salience;
}


/**
 * @brief Set fusion strategy
 *
 * WHAT: Change how modalities are combined
 * WHY:  Different strategies for different contexts
 * HOW:  Update strategy enum in evaluator
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param evaluator Salience evaluator
 * @param strategy Fusion strategy (MAX, WEIGHTED_AVG, LEARNED)
 * @return true on success
 */
bool salience_set_fusion_strategy(salience_evaluator_t evaluator,
                                  salience_fusion_strategy_t strategy)
{
    // Guard: Validate evaluator
    if (!evaluator) {
        salience_set_error("NULL evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_set_fusion_strategy: evaluator is NULL");
        return false;
    }

    // Guard: Validate strategy
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_set_fusion_strategy", 0.0f);


    if (strategy < SALIENCE_FUSION_MAX || strategy > SALIENCE_FUSION_LEARNED) {
        salience_set_error("Invalid fusion strategy: %d", strategy);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_set_fusion_strategy: validation failed");
        return false;
    }

    nimcp_mutex_lock(&evaluator->eval_lock);

    evaluator->fusion_strategy = strategy;

    nimcp_mutex_unlock(&evaluator->eval_lock);

    const char* strategy_names[] = {"MAX", "WEIGHTED_AVG", "LEARNED"};
    LOG_DEBUG("Set fusion strategy to %s", strategy_names[strategy]);

    return true;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void salience_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_salience_health_agent = agent;
    }
}
