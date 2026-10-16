// nimcp_introspection_part_accessors.c - accessors functions
// Part of nimcp_introspection.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_introspection.c

introspection_config_t introspection_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_default_config", 0.0f);


    introspection_config_t config = {.default_strategy = STATE_STRATEGY_BALANCED,
                                     .activity_threshold = 0.3F,
                                     .history_size = 100,
                                     .enable_pattern_tracking = true,
                                     .enable_uncertainty_estimation = true,
                                     .uncertainty_ensemble_size = 5,
                                     .enable_bio_async = false,
                                     .on_state_change = NULL,
                                     .callback_context = NULL,
                                     .enable_auto_history = false,
                                     .history_sample_interval_ms = 100,  /* 100ms default */
                                     .history_change_threshold = 0.05F}; /* 5% change threshold */
    return config;
}


/* ========================================================================
 * NEURON POPULATION QUERIES
 * ======================================================================== */

/**
 * WHAT: Get currently active neuron population
 * WHY: See which neurons are firing
 * HOW: Scan network, collect neurons above threshold
 *
 * COMPLEXITY: O(n) where n = network size
 * PERFORMANCE: ~0.1-1ms for typical networks
 */
neuron_population_t brain_get_active_population(introspection_context_t context, float threshold)
{
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_get_active_pop", 0.0f);


    neuron_population_t population;
    memset(&population, 0, sizeof(neuron_population_t));

    if (context == NULL) {
        return population;
    }

    nimcp_mutex_lock(&context->lock);
    context->stats.queries_total++;
    context->stats.queries_active_population++;
    nimcp_mutex_unlock(&context->lock);

    /* WHAT: Get brain's underlying adaptive network */
    /* WHY: Need direct access to neuron activations */
    adaptive_network_t network = brain_get_network(context->brain);
    if (network == NULL) {
        return population;
    }

    /* WHAT: Get network size */
    uint32_t total_neurons = adaptive_network_get_neuron_count(network);
    if (total_neurons == 0) {
        return population;
    }

    /* WHAT: Allocate temporary arrays for gathering active neurons */
    /* WHY: Don't know count until we scan, so use max size */
    uint32_t* temp_ids = (uint32_t*) nimcp_calloc(total_neurons, sizeof(uint32_t));
    float* temp_activations = (float*) nimcp_calloc(total_neurons, sizeof(float));

    if (temp_ids == NULL || temp_activations == NULL) {
        nimcp_free(temp_ids);
        temp_ids = NULL;
        nimcp_free(temp_activations);
        temp_activations = NULL;
        return population;
    }

    /* WHAT: Get active neurons from real network */
    /* HOW: Use new adaptive_network API that does the scan for us */
    uint32_t active_count = adaptive_network_get_active_neurons(network, threshold, temp_ids,
                                                                temp_activations, total_neurons);

    /* WHAT: Set population metadata */
    population.total_neurons = total_neurons;
    population.activity_threshold = threshold;
    population.timestamp = nimcp_time_monotonic_ms();
    population.num_active = active_count;

    if (active_count == 0) {
        nimcp_free(temp_ids);
        temp_ids = NULL;
        nimcp_free(temp_activations);
        temp_activations = NULL;
        return population;
    }

    /* WHAT: Allocate right-sized arrays for results */
    population.neuron_ids = (uint32_t*) nimcp_calloc(active_count, sizeof(uint32_t));
    population.activation_levels = (float*) nimcp_calloc(active_count, sizeof(float));

    if (population.neuron_ids == NULL || population.activation_levels == NULL) {
        nimcp_free(population.neuron_ids);
        nimcp_free(population.activation_levels);
        nimcp_free(temp_ids);
        temp_ids = NULL;
        nimcp_free(temp_activations);
        temp_activations = NULL;
        memset(&population, 0, sizeof(neuron_population_t));
        return population;
    }

    /* WHAT: Copy active neuron data to results */
    memcpy(population.neuron_ids, temp_ids, active_count * sizeof(uint32_t));
    memcpy(population.activation_levels, temp_activations, active_count * sizeof(float));

    /* WHAT: Free temporary arrays */
    nimcp_free(temp_ids);
    temp_ids = NULL;
    nimcp_free(temp_activations);
    temp_activations = NULL;

    return population;
}


/**
 * WHAT: Get detailed activity for specific neuron
 * WHY: Deep inspection of individual neuron
 * HOW: Access neuron structure, compute statistics
 *
 * COMPLEXITY: O(1)
 */
neuron_activity_t brain_get_neuron_activity(introspection_context_t context, uint32_t neuron_id)
{
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_get_neuron_act", 0.0f);


    neuron_activity_t activity;
    memset(&activity, 0, sizeof(neuron_activity_t));

    if (context == NULL) {
        return activity;
    }

    nimcp_mutex_lock(&context->lock);
    context->stats.queries_total++;
    nimcp_mutex_unlock(&context->lock);

    /* WHAT: Get brain's underlying network */
    adaptive_network_t network = brain_get_network(context->brain);
    if (network == NULL) {
        return activity;
    }

    /* WHAT: Fill in neuron metadata */
    activity.neuron_id = neuron_id;

    /* WHAT: Get real neuron activation from network */
    float raw_activation = 0.0f;
    if (!adaptive_network_get_neuron_activation(network, neuron_id, &raw_activation)) {
        return activity; /* Neuron doesn't exist */
    }

    /* WHAT: Normalize biological potential to 0-1 range */
    /* WHY: Neurons use biological potentials (-65 to +30 mV), but
     * introspection API should present normalized activations */
    /* HOW: Map [-65, +30] mV to [0, 1] */
    const float REST_POTENTIAL = -65.0F;
    const float PEAK_POTENTIAL = 30.0F;
    activity.activation = (raw_activation - REST_POTENTIAL) / (PEAK_POTENTIAL - REST_POTENTIAL);

    /* WHAT: Clamp to valid range */
    if (activity.activation < 0.0F)
        activity.activation = 0.0F;
    if (activity.activation > 1.0F)
        activity.activation = 1.0F;

    /* WHAT: Get connection count and total weight */
    adaptive_network_get_connection_count(network, neuron_id, &activity.num_connections);
    adaptive_network_get_total_weight(network, neuron_id, &activity.total_weight);

    /* WHAT: Compute derived properties */
    /* TODO: Get gradient from actual backprop when available */
    activity.gradient = 0.0F;                             /* Not tracked yet */
    activity.decision_contribution = activity.activation; /* Approximate */
    activity.is_active = activity.activation >= context->config.activity_threshold;

    return activity;
}


/* ========================================================================
 * INTERNAL STATE QUERIES
 * ======================================================================== */

/**
 * WHAT: Get compressed internal state representation
 * WHY: State vector for logging, comparison, transfer
 * HOW: Sample neurons based on strategy, compute entropy
 *
 * DESIGN PATTERN: Strategy (different sampling strategies)
 * COMPLEXITY: O(n*s) where s = sampling rate
 */
brain_state_t brain_get_internal_state(introspection_context_t context,
                                       state_extraction_strategy_t strategy)
{
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_get_internal_s", 0.0f);


    brain_state_t state;
    memset(&state, 0, sizeof(brain_state_t));

    if (context == NULL) {
        return state;
    }

    nimcp_mutex_lock(&context->lock);
    context->stats.queries_total++;
    context->stats.queries_internal_state++;
    nimcp_mutex_unlock(&context->lock);

    /* WHAT: Determine sampling rate based on strategy */
    /* WHY: Trade-off between speed and accuracy */
    float sampling_rate = 0.0f;
    const char* strategy_name;
    switch (strategy) {
        case STATE_STRATEGY_FAST:
            sampling_rate = 0.1F; /* 10% sampling */
            strategy_name = "fast";
            break;
        case STATE_STRATEGY_BALANCED:
            sampling_rate = 0.3F; /* 30% sampling */
            strategy_name = "balanced";
            break;
        case STATE_STRATEGY_DETAILED:
            sampling_rate = 1.0F; /* 100% sampling */
            strategy_name = "detailed";
            break;
        default:
            sampling_rate = 0.3F;
            strategy_name = "balanced";
    }

    /* WHAT: Get brain's underlying network */
    adaptive_network_t network = brain_get_network(context->brain);
    if (network == NULL) {
        return state;
    }

    /* WHAT: Get actual network size */
    uint32_t total_neurons = adaptive_network_get_neuron_count(network);
    uint32_t sampled_neurons = (uint32_t) (total_neurons * sampling_rate);

    if (sampled_neurons == 0) {
        sampled_neurons = 1; /* At least one sample */
    }

    /* WHAT: Allocate state vector */
    state.dimension = sampled_neurons;
    state.state_vector = (float*) nimcp_calloc(sampled_neurons, sizeof(float));
    if (state.state_vector == NULL) {
        return state;
    }

    /* WHAT: Sample neuron activations from real network */
    /* HOW: For fast/balanced, sample evenly spaced neurons */
    /*      For detailed, get all neurons */
    uint32_t stride = (uint32_t) (1.0F / sampling_rate);
    if (stride == 0)
        stride = 1;

    /* WHAT: Biological potential normalization constants */
    const float REST_POTENTIAL = -65.0F;
    const float PEAK_POTENTIAL = 30.0F;

    for (uint32_t i = 0; i < sampled_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sampled_neurons > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)sampled_neurons);
        }

        uint32_t neuron_id = i * stride;
        if (neuron_id >= total_neurons) {
            neuron_id = total_neurons - 1; /* Clamp to valid range */
        }

        /* WHAT: Get real neuron activation */
        float raw_activation = 0.0f;
        if (adaptive_network_get_neuron_activation(network, neuron_id, &raw_activation)) {
            /* WHAT: Normalize biological potential to 0-1 range */
            state.state_vector[i] =
                (raw_activation - REST_POTENTIAL) / (PEAK_POTENTIAL - REST_POTENTIAL);
            if (state.state_vector[i] < 0.0F)
                state.state_vector[i] = 0.0F;
            if (state.state_vector[i] > 1.0F)
                state.state_vector[i] = 1.0F;
        } else {
            state.state_vector[i] = 0.0F; /* Default if unavailable */
        }
    }

    /* WHAT: Compute information content (entropy) */
    /* WHY: Measure how much information is in this state */
    state.information_content = compute_introspection_entropy(state.state_vector, sampled_neurons);

    /* WHAT: Set metadata */
    state.compression_ratio = (float) total_neurons / (float) sampled_neurons;
    state.timestamp = nimcp_time_monotonic_ms();

    /* WHAT: Generate human-readable interpretation */
    /* WHY: Help developers understand what state means */
    char interp_buffer[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(interp_buffer, sizeof(interp_buffer),
             "State extracted using %s strategy (%.0f%% sampling), "
             "%.2f bits entropy, compression ratio %.2fx",
             strategy_name, sampling_rate * 100.0F, state.information_content,
             state.compression_ratio);
    state.interpretation = nimcp_strdup(interp_buffer);

    return state;
}


/* ========================================================================
 * UNCERTAINTY ESTIMATION
 * ======================================================================== */

/**
 * WHAT: Estimate uncertainty for a decision with emotional working memory context
 * WHY: Metacognition requires knowing when uncertain, emotions modulate confidence
 * HOW: Ensemble method + emotional context from working memory
 *
 * METHOD:
 * 1. Get predictions from ensemble of models
 * 2. Variance of predictions = epistemic uncertainty (model doesn't know)
 * 3. Entropy of each prediction = aleatoric uncertainty (data is noisy)
 * 4. Phase 10.3: Check emotional working memory for emotional context
 * 5. High arousal emotions increase uncertainty (biological: stress affects judgment)
 * 6. Negative valence increases epistemic uncertainty (doubt, fear)
 * 7. Positive high-arousal increases aleatoric uncertainty (excitement, overconfidence)
 *
 * BIOLOGICAL BASIS:
 * - High arousal states (stress, excitement) impair metacognitive accuracy
 * - Negative emotions bias toward uncertainty and doubt
 * - Positive emotions can lead to overconfidence
 * - Amygdala activation during emotion processing interferes with uncertainty estimation
 *
 * COMPLEXITY: O(k*m + w) where k=ensemble size, m=model complexity, w=working memory size
 * TIME: ~1-5ms depending on ensemble size
 */
brain_uncertainty_t brain_get_uncertainty(introspection_context_t context, const float* features,
                                          uint32_t num_features)
{
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_get_uncertaint", 0.0f);


    brain_uncertainty_t uncertainty;
    memset(&uncertainty, 0, sizeof(brain_uncertainty_t));

    if (context == NULL || features == NULL || num_features == 0) {
        return uncertainty;
    }

    if (!context->config.enable_uncertainty_estimation) {
        return uncertainty;
    }

    nimcp_mutex_lock(&context->lock);
    context->stats.queries_total++;
    context->stats.queries_uncertainty++;
    nimcp_mutex_unlock(&context->lock);

    /* WHAT: Check if ensemble is attached */
    /* WHY: Use real ensemble if available, fallback to heuristic */
    ensemble_context_t ensemble = context->ensemble;

    if (ensemble != NULL) {
        /* CASE 1: Real ensemble available - compute true uncertainty */
        LOG_DEBUG("Using real ensemble for uncertainty estimation");

        /* WHAT: Get uncertainty from ensemble */
        ensemble_uncertainty_result_t ens_result =
            ensemble_compute_uncertainty(ensemble, features, num_features);

        /* WHAT: Transfer ensemble results to brain uncertainty structure */
        uncertainty.epistemic = ens_result.epistemic;
        uncertainty.aleatoric = ens_result.aleatoric;
        uncertainty.total = ens_result.total;
        uncertainty.ensemble_size = ens_result.num_models;

        /* WHAT: Allocate and copy ensemble predictions */
        uncertainty.ensemble_predictions = (float*)
            nimcp_calloc(ens_result.num_models, sizeof(float));
        if (uncertainty.ensemble_predictions != NULL && ens_result.mean_prediction != NULL) {
            /* Copy mean prediction as representative ensemble output */
            /* For compatibility, store first output dimension from each model */
            for (uint32_t i = 0; i < ens_result.num_models && i < ens_result.num_models; i++) {
                if (ens_result.individual_predictions != NULL &&
                    ens_result.individual_predictions[i].prediction != NULL &&
                    ens_result.individual_predictions[i].size > 0) {
                    uncertainty.ensemble_predictions[i] =
                        ens_result.individual_predictions[i].prediction[0];
                } else {
                    uncertainty.ensemble_predictions[i] = 0.0F;
                }
            }
        }

        /* WHAT: Clean up ensemble result */
        ensemble_uncertainty_free(&ens_result);

    } else {
        /* CASE 2: No ensemble - use improved heuristic based on network state */
        LOG_WARN("No ensemble attached - using heuristic uncertainty estimation");

        /* WHAT: Get ensemble size from config for fallback */
        uint32_t ensemble_size = context->config.uncertainty_ensemble_size;
        uncertainty.ensemble_size = ensemble_size;

        /* Guard: ensemble_size == 0 would cause division by zero and zero-size allocation */
        if (ensemble_size == 0) {
            uncertainty.epistemic = 1.0f;
            uncertainty.aleatoric = 1.0f;
            return uncertainty;
        }

        /* WHAT: Allocate array for simulated predictions */
        uncertainty.ensemble_predictions = (float*)
            nimcp_calloc(ensemble_size, sizeof(float));
        if (uncertainty.ensemble_predictions == NULL) {
            return uncertainty;
        }

        /* WHAT: Use network state to estimate uncertainty */
        /* HOW: Base uncertainty on network activation statistics */
        adaptive_network_t network = brain_get_network(context->brain);
        float base_prediction = 0.5F;  /* Default neutral prediction */
        float variance_estimate = 0.1F; /* Default variance */

        if (network != NULL) {
            /* WHAT: Get network activity to inform uncertainty */
            neuron_population_t pop = brain_get_active_population(context, 0.3F);
            if (pop.num_active > 0) {
                /* Base prediction on network activity */
                float activity_ratio = (float) pop.num_active / (float) pop.total_neurons;
                base_prediction = activity_ratio;

                /* Variance based on activity distribution */
                if (pop.activation_levels != NULL) {
                    float mean_act = 0.0F;
                    for (uint32_t i = 0; i < pop.num_active; i++) {
                        /* Phase 8: Loop progress heartbeat */
                        if ((i & 0xFF) == 0 && pop.num_active > 256) {
                            introspection_heartbeat("introspectio_loop",
                                             (float)(i + 1) / (float)pop.num_active);
                        }

                        mean_act += pop.activation_levels[i];
                    }
                    mean_act /= pop.num_active;

                    float var = 0.0F;
                    for (uint32_t i = 0; i < pop.num_active; i++) {
                        /* Phase 8: Loop progress heartbeat */
                        if ((i & 0xFF) == 0 && pop.num_active > 256) {
                            introspection_heartbeat("introspectio_loop",
                                             (float)(i + 1) / (float)pop.num_active);
                        }

                        float diff = pop.activation_levels[i] - mean_act;
                        var += diff * diff;
                    }
                    variance_estimate = sqrtf(var / pop.num_active);
                }
            }
            neuron_population_free(&pop);
        }

        /* WHAT: Generate simulated ensemble predictions */
        float mean_prediction = 0.0F;
        for (uint32_t i = 0; i < ensemble_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ensemble_size > 256) {
                introspection_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)ensemble_size);
            }

            /* Add noise based on estimated variance */
            /* P1-COG-02: Use thread-safe mc_random_uniform instead of rand() */
            float noise = (mc_random_uniform(&context->rand_seed) - 0.5F) * variance_estimate * 2.0F;
            uncertainty.ensemble_predictions[i] = base_prediction + noise;
            /* Clamp to [0, 1] */
            if (uncertainty.ensemble_predictions[i] < 0.0F)
                uncertainty.ensemble_predictions[i] = 0.0F;
            if (uncertainty.ensemble_predictions[i] > 1.0F)
                uncertainty.ensemble_predictions[i] = 1.0F;
            mean_prediction += uncertainty.ensemble_predictions[i];
        }
        mean_prediction /= ensemble_size;

        /* WHAT: Compute epistemic uncertainty (variance of predictions) */
        float variance = 0.0F;
        for (uint32_t i = 0; i < ensemble_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ensemble_size > 256) {
                introspection_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)ensemble_size);
            }

            float diff = uncertainty.ensemble_predictions[i] - mean_prediction;
            variance += diff * diff;
        }
        variance /= ensemble_size;
        uncertainty.epistemic = sqrtf(variance);

        /* WHAT: Compute aleatoric uncertainty (entropy of mean prediction) */
        float p = mean_prediction;
        if (p < 1e-6F)
            p = 1e-6F;
        if (p > 1.0F - 1e-6F)
            p = 1.0F - 1e-6F;
        uncertainty.aleatoric = -(p * log2f(p) + (1.0F - p) * log2f(1.0F - p));
    }

    /* =========================================================================
     * Immune system modulation of uncertainty
     * =========================================================================
     * WHAT: Immune system activity increases uncertainty
     * WHY:  Active immune response indicates system instability
     * HOW:  Antibody effectiveness affects confidence in predictions
     */
    brain_immune_system_t* immune = introspection_get_immune(context);
    if (immune != NULL) {
        brain_immune_stats_t stats;
        if (brain_immune_get_stats(immune, &stats) == 0) {
            /* WHAT: Active immune responses increase uncertainty */
            /* WHY: System under attack cannot make reliable predictions */
            /* HOW: Scale uncertainty based on active antibody count */
            if (stats.active_antibodies > 0) {
                float immune_factor = 1.0F + ((float)stats.active_antibodies / 10.0F) * 0.3F;
                if (immune_factor > 1.5F) immune_factor = 1.5F; /* Cap at 50% increase */

                uncertainty.epistemic *= immune_factor;
                uncertainty.aleatoric *= immune_factor;

                LOG_DEBUG("Immune modulation: %u active antibodies, factor=%.2f",
                         stats.active_antibodies, immune_factor);
            }

            /* WHAT: Low system health increases uncertainty */
            /* WHY: Compromised system has unreliable state */
            if (stats.system_health < 0.7F) {
                float health_penalty = (0.7F - stats.system_health) * 0.5F;
                uncertainty.epistemic += health_penalty;
            }
        }
    }

    /* =========================================================================
     * PHASE 10.3: Emotional working memory modulation of uncertainty
     * =========================================================================
     * WHAT: Retrieve emotional context from working memory to adjust uncertainty
     * WHY:  Emotional state affects metacognitive accuracy and confidence
     * HOW:  High arousal → increase uncertainty, negative valence → bias toward doubt
     */
    working_memory_t* wm = brain_get_working_memory(context->brain);

    if (wm && working_memory_get_size(wm) > 0) {
        /* WHAT: Scan working memory for high-intensity emotional items */
        /* WHY:  Recent emotional experiences modulate current uncertainty perception */

        float max_arousal = 0.0F;
        float avg_valence = 0.0F;
        uint32_t emotional_count = 0;
        uint32_t wm_size = working_memory_get_size(wm);

        /* WHAT: Aggregate emotional context from working memory items */
        for (uint32_t i = 0; i < wm_size && i < 7; i++) {  // Miller's 7±2: check up to 7 items
            emotional_tag_t emotion;
            if (working_memory_get_emotion(wm, i, &emotion) && emotion.intensity > 0.3F) {
                /* WHAT: Track highest arousal and average valence */
                if (emotion.arousal > max_arousal) {
                    max_arousal = emotion.arousal;
                }
                avg_valence += emotion.valence;
                emotional_count++;
            }
        }

        if (emotional_count > 0) {
            avg_valence /= emotional_count;

            /* WHAT: High arousal increases both epistemic and aleatoric uncertainty */
            /* WHY:  Arousal impairs metacognitive accuracy (Bolte et al., 2003) */
            /* HOW:  Multiply uncertainty by (1.0 + arousal * 0.5) */
            if (max_arousal > 0.5F) {
                float arousal_boost = 1.0F + (max_arousal - 0.5F) * 0.5F;  // Up to 1.25x at max arousal
                uncertainty.epistemic *= arousal_boost;
                uncertainty.aleatoric *= arousal_boost;
            }

            /* WHAT: Negative valence biases toward epistemic uncertainty (doubt) */
            /* WHY:  Negative emotions increase self-doubt and model uncertainty */
            /* HOW:  Boost epistemic uncertainty for negative valence */
            if (avg_valence < -0.3F) {
                uncertainty.epistemic *= (1.0F + fabsf(avg_valence) * 0.3F);  // Up to 1.3x
            }

            /* WHAT: Positive high-arousal increases aleatoric uncertainty */
            /* WHY:  Excitement/joy can lead to overconfidence and noise */
            /* HOW:  Boost aleatoric for positive + high arousal combination */
            if (avg_valence > 0.3F && max_arousal > 0.6F) {
                uncertainty.aleatoric *= 1.2F;  // 20% increase
            }
        }
    }

    /* WHAT: Combine uncertainties (now emotionally modulated) */
    /* WHY: Total uncertainty is sum of epistemic and aleatoric */
    uncertainty.total = uncertainty.epistemic + uncertainty.aleatoric;
    if (uncertainty.total > 1.0F)
        uncertainty.total = 1.0F;

    uncertainty.confidence = 1.0F - uncertainty.total;

    return uncertainty;
}


/**
 * WHAT: Check if pattern is currently active
 * WHY: Query specific learned patterns
 * HOW: Hash lookup, check activity threshold
 *
 * COMPLEXITY: O(1)
 */
bool brain_is_pattern_active(introspection_context_t context, const char* pattern_name)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_is_pattern_act", 0.0f);


    if (context && context->bio_ctx) {
        bio_router_process_inbox(context->bio_ctx, 5);
    }

    if (context == NULL || pattern_name == NULL) {
        return false;
    }

    if (!context->config.enable_pattern_tracking || context->pattern_registry == NULL) {
        /* Feature disabled or registry not initialized - normal behavior, not an error */
        return false;
    }

    nimcp_mutex_lock(&context->lock);
    context->stats.queries_total++;
    context->stats.queries_pattern++;
    nimcp_mutex_unlock(&context->lock);

    nimcp_mutex_lock(&context->pattern_registry->lock);
    pattern_entry_t* entry = pattern_registry_lookup(context->pattern_registry, pattern_name);
    bool is_active =
        (entry != NULL && entry->current_activity >= context->config.activity_threshold);
    nimcp_mutex_unlock(&context->pattern_registry->lock);

    return is_active;
}


/**
 * WHAT: Get detailed pattern information
 * WHY: Inspect pattern metadata
 * HOW: Lookup and copy pattern entry
 *
 * COMPLEXITY: O(1)
 */
pattern_info_t* brain_get_pattern_info(introspection_context_t context, const char* pattern_name)
{
    if (context == NULL || pattern_name == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_pattern_info: validation failed");
        return NULL;
    }

    if (!context->config.enable_pattern_tracking || context->pattern_registry == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_pattern_info: context->config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_get_pattern_in", 0.0f);


    nimcp_mutex_lock(&context->pattern_registry->lock);
    pattern_entry_t* entry = pattern_registry_lookup(context->pattern_registry, pattern_name);

    if (entry == NULL) {
        nimcp_mutex_unlock(&context->pattern_registry->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_pattern_info: validation failed");
        return NULL;
    }

    /* WHAT: Allocate and populate info structure */
    pattern_info_t* info = (pattern_info_t*) nimcp_malloc(sizeof(pattern_info_t));
    if (info == NULL) {
        nimcp_mutex_unlock(&context->pattern_registry->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_pattern_info: validation failed");
        return NULL;
    }

    info->pattern_name = nimcp_strdup(entry->name);
    info->current_activity = entry->current_activity;
    info->average_activity = (entry->activation_count > 0) ?
        entry->activity_sum / entry->activation_count : 0.0f;
    info->pattern_strength = entry->pattern_strength;
    info->activation_count = entry->activation_count;
    info->first_learned = entry->first_learned;
    info->last_activated = entry->last_activated;

    nimcp_mutex_unlock(&context->pattern_registry->lock);

    return info;
}


network_topology_t brain_get_topology(introspection_context_t context)
{
    /* Guard clause: validate context */
    if (context == NULL) {
        network_topology_t empty;
        memset(&empty, 0, sizeof(network_topology_t));
        return empty;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_get_topology", 0.0f);


    nimcp_mutex_lock(&context->lock);

    /* CASE 1: Return cached topology (most common path) */
    /* DESIGN PATTERN: Lazy Initialization with memoization */
    if (context->topology_cached) {
        network_topology_t result = clone_topology(&context->topology);
        nimcp_mutex_unlock(&context->lock);
        return result;
    }

    /* CASE 2: Build and cache new topology (first call only) */
    network_topology_t topology = build_topology(context->brain);

    /* Cache with independent copy to prevent double-free */
    context->topology = clone_topology(&topology);
    context->topology_cached = true;

    nimcp_mutex_unlock(&context->lock);

    return topology;
}


/**
 * WHAT: Get recent activity history
 * WHY: Track brain state evolution
 * HOW: Dequeue all entries from activity queue
 *
 * REFACTORING NOTE:
 * - Replaced custom circular buffer traversal with nimcp_queue operations
 * - SIMPLIFIED: ~30 lines of modulo arithmetic → simple dequeue loop
 * - BEHAVIOR CHANGE: Now consumes (empties) the queue when called
 *   WHY: nimcp_queue doesn't have peek_all, and this matches "get history" semantics
 *   IMPACT: Low - function is rarely called, and getting history to examine it makes sense
 *
 * COMPLEXITY: O(h) where h = history size
 * THREAD-SAFE: Yes (queue operations are thread-safe)
 *
 * @param context Introspection context
 * @param num_entries Output: number of history entries returned
 * @return Array of history entries (must be freed by caller) or NULL if empty
 */
activity_history_entry_t* brain_get_activity_history(introspection_context_t context,
                                                     uint32_t* num_entries)
{
    if (context == NULL || num_entries == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_activity_history: validation failed");
        return NULL;
    }

    // Get current queue size
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_get_activity_h", 0.0f);


    size_t queue_size = nimcp_queue_get_size(context->activity_queue);
    *num_entries = (uint32_t) queue_size;

    if (queue_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_activity_history: queue_size is zero");
        return NULL;  // No history available
    }

    // Allocate array for history entries
    activity_history_entry_t* history =
        (activity_history_entry_t*) nimcp_calloc(queue_size, sizeof(activity_history_entry_t));

    if (history == NULL) {
        *num_entries = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_activity_history: validation failed");
        return NULL;
    }

    // Dequeue all entries (empties the queue)
    // WHY CONSUME: No peek_all API, and "get history" implies reading it out
    for (uint32_t i = 0; i < queue_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && queue_size > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)queue_size);
        }

        nimcp_result_t result = nimcp_queue_dequeue(context->activity_queue, &history[i], 0);
        if (result != NIMCP_SUCCESS) {
            // Partial failure - return what we got
            *num_entries = i;
            if (i == 0) {
                nimcp_free(history);
                history = NULL;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_activity_history: i is zero");
                return NULL;
            }
            break;
        }
    }

    return history;
}


/* ========================================================================
 * ENSEMBLE UNCERTAINTY INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Attach ensemble to introspection context
 * WHY: Enable real uncertainty estimation in brain_get_uncertainty()
 * HOW: Store ensemble reference in context
 *
 * COMPLEXITY: O(1)
 */
bool introspection_set_ensemble(introspection_context_t context,
                                ensemble_context_t ensemble)
{
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_set_ensemble: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_set_ensemble", 0.0f);


    nimcp_mutex_lock(&context->lock);
    context->ensemble = ensemble;
    nimcp_mutex_unlock(&context->lock);

    LOG_INFO("Ensemble attached to introspection context (%u models)",
            ensemble ? ensemble_get_size(ensemble) : 0);

    return true;
}


/**
 * WHAT: Get ensemble from introspection context
 * WHY: Access ensemble for manual uncertainty computation
 * HOW: Return ensemble reference
 *
 * COMPLEXITY: O(1)
 */
ensemble_context_t introspection_get_ensemble(introspection_context_t context)
{
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_ensemble: validation failed");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_get_ensemble", 0.0f);


    nimcp_mutex_lock(&context->lock);
    ensemble_context_t ensemble = context->ensemble;
    nimcp_mutex_unlock(&context->lock);

    return ensemble;
}


/* ========================================================================
 * STATISTICS
 * ======================================================================== */

/**
 * WHAT: Get introspection statistics
 * WHY: Monitor performance
 * HOW: Copy stats structure
 *
 * COMPLEXITY: O(1)
 */
bool introspection_get_stats(introspection_context_t context, introspection_stats_t* stats)
{
    if (context == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_get_stats: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_get_stats", 0.0f);


    nimcp_mutex_lock(&context->lock);
    *stats = context->stats;
    nimcp_mutex_unlock(&context->lock);

    return true;
}


/**
 * WHAT: Set activity sampling interval
 * WHY: Allow runtime adjustment of sampling rate
 * HOW: Updates configuration with mutex protection
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_set_sample_interval(introspection_context_t context, uint32_t interval_ms)
{
    /* WHAT: Validate input */
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_set_sample_interval: validation failed");
        return false;
    }

    /* WHAT: Update interval with mutex protection */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_set_sample_interval", 0.0f);


    nimcp_mutex_lock(&context->lock);
    context->config.history_sample_interval_ms = interval_ms;
    nimcp_mutex_unlock(&context->lock);

    return true;
}


/**
 * WHAT: Get activity history buffer statistics
 * WHY: Monitor history usage and performance
 * HOW: Query queue statistics and compute utilization
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_get_history_stats(introspection_context_t context,
                                     uint32_t* current_size,
                                     uint32_t* capacity,
                                     float* utilization)
{
    /* WHAT: Validate inputs */
    if (context == NULL || current_size == NULL || capacity == NULL || utilization == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_get_history_stats: validation failed");
        return false;
    }

    /* WHAT: Get queue statistics */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_get_history_stats", 0.0f);


    size_t size = nimcp_queue_get_size(context->activity_queue);
    size_t cap = nimcp_queue_get_capacity(context->activity_queue);

    /* WHAT: Populate outputs */
    *current_size = (uint32_t) size;
    *capacity = (uint32_t) cap;
    *utilization = cap > 0 ? (float) size / (float) cap : 0.0F;

    return true;
}


/**
 * WHAT: Register callback for activity sampling events
 * WHY: Allow external observers to react to each activity snapshot
 * HOW: Store callback and context with mutex protection
 *
 * DESIGN PATTERN: Observer (callback registration)
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_set_activity_callback(introspection_context_t context,
                                         activity_sample_callback_t callback,
                                         void* user_data)
{
    /* WHAT: Validate input */
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_set_activity_callback: validation failed");
        return false;
    }

    /* WHAT: Update callback with mutex protection */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_set_activity_callbac", 0.0f);


    nimcp_mutex_lock(&context->lock);
    context->sample_callback = callback;
    context->sample_callback_context = user_data;
    nimcp_mutex_unlock(&context->lock);

    return true;
}


brain_kg_node_list_t* introspection_get_active_modules(introspection_context_t context)
{
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_active_modules: validation failed");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_get_active_modules", 0.0f);


    nimcp_mutex_lock(&context->lock);

    /* Check if KG is connected */
    if (!context->kg_connected || !kg_is_available(&context->kg_context)) {
        nimcp_mutex_unlock(&context->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_active_modules: required parameter is NULL (context->kg_connected, kg_is_available)");
        return NULL;
    }

    /* Query KG for all nodes in ACTIVE state */
    brain_kg_node_list_t* active_nodes = kg_get_nodes_by_state_safe(
        &context->kg_context,
        BRAIN_KG_STATE_ACTIVE
    );

    nimcp_mutex_unlock(&context->lock);
    return active_nodes;
}


brain_kg_node_list_t* introspection_get_module_topology(
    introspection_context_t context,
    const char* center_name,
    uint32_t max_depth
)
{
    if (context == NULL || center_name == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_module_topology: validation failed");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_get_module_topology", 0.0f);


    nimcp_mutex_lock(&context->lock);

    /* Check if KG is connected */
    if (!context->kg_connected || !kg_is_available(&context->kg_context)) {
        nimcp_mutex_unlock(&context->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_module_topology: required parameter is NULL (context->kg_connected, kg_is_available)");
        return NULL;
    }

    /* Find the center node */
    brain_kg_node_id_t center_id = kg_find_node_safe(&context->kg_context, center_name);
    if (center_id == BRAIN_KG_INVALID_NODE) {
        nimcp_mutex_unlock(&context->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_module_topology: validation failed");
        return NULL;
    }

    /* Create temporary context for center node to use reachable helper */
    kg_module_context_t center_ctx = context->kg_context;
    center_ctx.self_node_id = center_id;

    brain_kg_node_list_t* reachable = kg_get_reachable_safe(&center_ctx, max_depth);

    nimcp_mutex_unlock(&context->lock);
    return reachable;
}


bool introspection_is_module_set_active(
    introspection_context_t context,
    const char** module_names,
    uint32_t count
)
{
    if (context == NULL || module_names == NULL || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_is_module_set_active: count is zero");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_is_module_set_active", 0.0f);


    nimcp_mutex_lock(&context->lock);

    /* Check if KG is connected */
    if (!context->kg_connected || !kg_is_available(&context->kg_context)) {
        nimcp_mutex_unlock(&context->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_is_module_set_active: required parameter is NULL (context->kg_connected, kg_is_available)");
        return false;  /* Can't check - assume not active */
    }

    /* Check each module */
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)count);
        }

        if (module_names[i] == NULL) {
            nimcp_mutex_unlock(&context->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_is_module_set_active: validation failed");
            return false;
        }

        /* Find the node */
        brain_kg_node_id_t node_id = kg_find_node_safe(
            &context->kg_context,
            module_names[i]
        );

        if (node_id == BRAIN_KG_INVALID_NODE) {
            nimcp_mutex_unlock(&context->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_is_module_set_active: validation failed");
            return false;  /* Module not found */
        }

        /* Get node and check state */
        const brain_kg_node_t* node = kg_get_node_safe(&context->kg_context, node_id);
        if (node == NULL || node->state != BRAIN_KG_STATE_ACTIVE) {
            nimcp_mutex_unlock(&context->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_is_module_set_active: validation failed");
            return false;  /* Module not active */
        }
    }

    nimcp_mutex_unlock(&context->lock);
    return true;  /* All modules are active */
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

/**
 * @brief Set instance-level health agent for introspection module
 */
void introspection_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_introspection_instance_health_agent = agent;
}
