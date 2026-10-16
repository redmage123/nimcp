// nimcp_salience_part_helpers.c - helpers functions
// Part of nimcp_salience.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_salience.c


/**
 * WHAT: Add entry to history
 * WHY: Update recent input history for novelty detection
 * HOW: Overwrite oldest entry (circular buffer) using fixed-size array
 */
static void history_buffer_add(history_buffer_t* hist, const float* features, uint32_t num_features,
                               uint64_t timestamp)
{
    /**
     * WHAT: Validate feature count
     * WHY: Fixed-size array has maximum capacity
     */
    if (num_features > SALIENCE_MAX_FEATURES) {
        return;  // Silently reject oversized inputs
    }

    nimcp_mutex_lock(&hist->lock);

    history_entry_t* entry = &hist->entries[hist->head];

    /**
     * WHAT: Copy features directly to fixed-size array
     * WHY: No heap allocation needed - eliminates malloc/free overhead
     * HOW: Direct memcpy to stack-allocated array
     */
    memcpy(entry->features, features, num_features * sizeof(float));
    entry->num_features = num_features;
    entry->timestamp = timestamp;
    entry->valid = true;

    /**
     * WHAT: Advance circular buffer head
     * WHY: Oldest entries are automatically overwritten
     */
    hist->head = (hist->head + 1) % hist->capacity;
    if (hist->count < hist->capacity) {
        hist->count++;
    }

    nimcp_mutex_unlock(&hist->lock);
}

static inline float simd_dot_product(const float* a, const float* b, uint32_t n)
{
#if defined(__AVX2__)
    /**
     * WHAT: AVX2 implementation - process 8 floats at a time
     * WHY: 8-wide SIMD for maximum throughput on modern CPUs
     * HOW: Use 256-bit AVX2 registers and instructions
     */
    __m256 sum = _mm256_setzero_ps();
    uint32_t i = 0;

    // Main loop: process 8 elements at a time
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);  // Load 8 floats from a (unaligned)
        __m256 vb = _mm256_loadu_ps(&b[i]);  // Load 8 floats from b (unaligned)
        sum = _mm256_fmadd_ps(va, vb, sum);  // sum += va * vb (fused multiply-add)
    }

    // Horizontal sum: reduce 8 lanes to single value
    __m128 sum_high = _mm256_extractf128_ps(sum, 1);  // Extract upper 128 bits
    __m128 sum_low = _mm256_castps256_ps128(sum);     // Extract lower 128 bits
    __m128 sum128 = _mm_add_ps(sum_low, sum_high);    // Add both halves

    sum128 = _mm_hadd_ps(sum128, sum128);  // Horizontal add: [a+b, c+d, a+b, c+d]
    sum128 = _mm_hadd_ps(sum128, sum128);  // Horizontal add: [a+b+c+d, ...]

    float result = 0.0f;
    _mm_store_ss(&result, sum128);  // Store lowest float

    // Tail loop: handle remaining elements (scalar)
    for (; i < n; i++) {
        result += a[i] * b[i];
    }

    return result;

#elif defined(__SSE2__)
    /**
     * WHAT: SSE2 implementation - process 4 floats at a time
     * WHY: 4-wide SIMD for compatibility with older CPUs
     * HOW: Use 128-bit SSE2 registers and instructions
     */
    __m128 sum = _mm_setzero_ps();
    uint32_t i = 0;

    // Main loop: process 4 elements at a time
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(&a[i]);  // Load 4 floats from a (unaligned)
        __m128 vb = _mm_loadu_ps(&b[i]);  // Load 4 floats from b (unaligned)
        sum = _mm_add_ps(sum, _mm_mul_ps(va, vb));  // sum += va * vb
    }

    // Horizontal sum: reduce 4 lanes to single value
    sum = _mm_hadd_ps(sum, sum);  // [a+b, c+d, a+b, c+d]
    sum = _mm_hadd_ps(sum, sum);  // [a+b+c+d, ...]

    float result = 0.0f;
    _mm_store_ss(&result, sum);  // Store lowest float

    // Tail loop: handle remaining elements (scalar)
    for (; i < n; i++) {
        result += a[i] * b[i];
    }

    return result;
#endif
}


/**
 * WHAT: SIMD-optimized L2 norm computation
 * WHY: Required for cosine similarity normalization
 * HOW: Vectorized sum of squares, then scalar sqrt
 *
 * @param vec Vector to compute norm for
 * @param n Number of elements
 * @return L2 norm (magnitude) of vector
 */
static inline float simd_norm(const float* vec, uint32_t n)
{
#if defined(__AVX2__)
    /**
     * WHAT: AVX2 implementation - process 8 floats at a time
     * WHY: Maximize throughput on modern CPUs
     */
    __m256 sum_sq = _mm256_setzero_ps();
    uint32_t i = 0;

    // Main loop: process 8 elements at a time
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(&vec[i]);
        sum_sq = _mm256_fmadd_ps(v, v, sum_sq);  // sum_sq += v * v
    }

    // Horizontal sum
    __m128 sum_high = _mm256_extractf128_ps(sum_sq, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum_sq);
    __m128 sum128 = _mm_add_ps(sum_low, sum_high);

    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);

    float result = 0.0f;
    _mm_store_ss(&result, sum128);

    // Tail loop: handle remaining elements
    for (; i < n; i++) {
        result += vec[i] * vec[i];
    }

    return sqrtf(result);

#elif defined(__SSE2__)
    /**
     * WHAT: SSE2 implementation - process 4 floats at a time
     * WHY: Good compatibility across CPUs
     */
    __m128 sum_sq = _mm_setzero_ps();
    uint32_t i = 0;

    // Main loop: process 4 elements at a time
    for (; i + 4 <= n; i += 4) {
        __m128 v = _mm_loadu_ps(&vec[i]);
        sum_sq = _mm_add_ps(sum_sq, _mm_mul_ps(v, v));
    }

    // Horizontal sum
    sum_sq = _mm_hadd_ps(sum_sq, sum_sq);
    sum_sq = _mm_hadd_ps(sum_sq, sum_sq);

    float result = 0.0f;
    _mm_store_ss(&result, sum_sq);

    // Tail loop: handle remaining elements
    for (; i < n; i++) {
        result += vec[i] * vec[i];
    }

    return sqrtf(result);
#endif
}


/**
 * WHAT: SIMD-optimized cosine distance computation
 * WHY: Novelty detection bottleneck - vectorize for 4-8x speedup
 * HOW: Vectorized dot product and norms, then scalar division
 *
 * ALGORITHM: cosine_distance = 1.0 - (dot(a,b) / (norm(a) * norm(b)))
 *
 * PERFORMANCE TARGET:
 * - SSE2: 4x speedup over scalar implementation
 * - AVX2: 8x speedup over scalar implementation
 *
 * @param a First vector
 * @param b Second vector
 * @param n Number of elements
 * @return Cosine distance [0, 2]
 */
static inline float simd_cosine_distance(const float* a, const float* b, uint32_t n)
{
    /**
     * WHAT: Compute all three components in parallel where possible
     * WHY: Minimize memory passes and maximize instruction throughput
     * HOW: Single pass for dot product, separate passes for norms
     */

    // Compute dot product
    float dot = simd_dot_product(a, b, n);

    // Compute norms
    float norm_a = simd_norm(a, n);
    float norm_b = simd_norm(b, n);

    /**
     * WHAT: Handle edge cases with epsilon guard
     * WHY: Prevent division by zero, define behavior for zero vectors
     * HOW: Same logic as scalar implementation
     */
    float denom = norm_a * norm_b;

    if (denom < NIMCP_VECTOR_EPSILON) {
        // Both zero = perfect match (distance = 0)
        if (norm_a < NIMCP_VECTOR_EPSILON && norm_b < NIMCP_VECTOR_EPSILON) {
            return 0.0F;
        }
        // One zero, one non-zero = maximum dissimilarity (distance = 1)
        return 1.0F;
    }

    // Cosine similarity = dot / (norm_a * norm_b)
    float cosine_sim = dot / denom;

    // Cosine distance = 1 - cosine_similarity
    return 1.0F - cosine_sim;
}

static float history_buffer_compute_novelty(history_buffer_t* hist, const float* features,
                                            uint32_t num_features)
{
    nimcp_mutex_lock(&hist->lock);

    if (hist->count == 0) {
        /**
         * WHAT: No history yet
         * WHY: Everything is novel when we have no context
         * RETURN: Maximum novelty
         */
        nimcp_mutex_unlock(&hist->lock);
        return 1.0F;
    }

    /**
     * WHAT: Find minimum distance to any historical entry
     * WHY: Novelty = distance to nearest similar input
     * HOW: Compute cosine distance to each entry, take minimum
     */
    float min_distance = 2.0F;  // Max cosine distance is 2.0

    for (uint32_t i = 0; i < hist->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hist->count > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)hist->count);
        }

        history_entry_t* entry = &hist->entries[i];
        if (!entry->valid || entry->num_features != num_features) {
            continue;
        }

        /**
         * WHAT: Compute cosine distance using vector utilities
         * WHY: Scale-invariant similarity metric
         * HOW: Use nimcp_vector_cosine_distance for consistency
         */
        float distance = nimcp_vector_cosine_distance(features, entry->features, num_features);

        if (distance < min_distance) {
            min_distance = distance;
        }
    }

    nimcp_mutex_unlock(&hist->lock);

    /**
     * WHAT: Normalize distance to 0-1
     * WHY: Cosine distance is 0-2, we want 0-1
     * HOW: Divide by 2, clamp to [0, 1]
     */
    float novelty = min_distance / 2.0F;
    novelty = fminf(1.0F, fmaxf(0.0F, novelty));

    return novelty;
}


/**
 * WHAT: Clear history buffer
 * WHY: Reset novelty detection
 * HOW: Simply mark all entries as invalid and reset counters
 */
static void history_buffer_clear(history_buffer_t* hist)
{
    nimcp_mutex_lock(&hist->lock);

    /**
     * WHAT: Mark all entries as invalid
     * WHY: No memory cleanup needed - features are fixed-size arrays
     * NOTE: Much simpler than before - no per-entry nimcp_free()
     */
    for (uint32_t i = 0; i < hist->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hist->count > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)hist->count);
        }

        hist->entries[i].valid = false;
    }

    hist->head = 0;
    hist->count = 0;

    nimcp_mutex_unlock(&hist->lock);
}


/**
 * WHAT: Compute surprise (prediction error)
 * WHY: Surprise = |actual - predicted|
 * HOW: Mean absolute error between prediction and actual
 *
 * BUGFIX: Added num_features parameter for bounds checking
 * WHY: Prevent buffer overflow when features array is smaller than expected
 *
 * @return Surprise score (0.0 = expected, 1.0 = totally unexpected)
 */
static float predictor_compute_surprise(predictor_t* pred, const float* features, uint32_t num_features)
{
    if (!pred->initialized) {
        return 0.5F;  // Moderate surprise when no prediction exists
    }

    nimcp_mutex_lock(&pred->lock);

    // BUGFIX: Use minimum of expected and actual sizes to prevent buffer overflow
    uint32_t safe_count = (num_features < pred->num_features) ? num_features : pred->num_features;

    /**
     * WHAT: Compute mean absolute prediction error
     * WHY: Measure of unexpectedness
     * HOW: Sum |actual - predicted| / num_features
     */
    float total_error = 0.0F;
    for (uint32_t i = 0; i < safe_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && safe_count > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)safe_count);
        }

        total_error += fabsf(features[i] - pred->prediction[i]);
    }

    float mae = (safe_count > 0) ? (total_error / safe_count) : 0.0f;

    nimcp_mutex_unlock(&pred->lock);

    /**
     * WHAT: Normalize to 0-1
     * WHY: Features have different scales
     * HOW: Assume features in [-1, 1] range, so max error is 2
     */
    float surprise = fminf(1.0F, mae / 2.0F);

    return surprise;
}

static brain_salience_t compute_salience_fast(salience_evaluator_t eval, const float* features,
                                              uint32_t num_features, uint64_t timestamp)
{
    brain_salience_t salience = {0};

    /**
     * WHAT: Fast novelty (only check against last N=5 entries)
     * WHY: Full history scan is expensive
     */
    if (eval->config.enable_novelty) {
        salience.novelty = history_buffer_compute_novelty(eval->history, features, num_features);
    }

    /**
     * WHAT: Fast surprise (simple threshold check)
     * WHY: Full prediction error calculation is expensive
     */
    if (eval->config.enable_surprise && eval->predictor->initialized) {
        salience.surprise = predictor_compute_surprise(eval->predictor, features, num_features);
    }

    /**
     * WHAT: Fast urgency (baseline only)
     * WHY: Learned urgency patterns require full network
     */
    if (eval->config.enable_urgency) {
        salience.urgency = eval->config.urgency_baseline;
    }

    /**
     * WHAT: Combine scores using weights
     * WHY: Different metrics have different importance
     * HOW: Weighted average
     */
    float total_weight =
        eval->config.novelty_weight + eval->config.surprise_weight + eval->config.urgency_weight;

    if (total_weight > 0.0F) {
        salience.salience = (salience.novelty * eval->config.novelty_weight +
                             salience.surprise * eval->config.surprise_weight +
                             salience.urgency * eval->config.urgency_weight) /
                            total_weight;
    }

    salience.confidence = 0.7F;      // Lower confidence for fast mode
    salience.estimated_cost = 0.1F;  // Low cost estimate

    return salience;
}


/**
 * WHAT: Balanced salience computation
 * WHY: Good speed-accuracy tradeoff
 * HOW: Full novelty + surprise, simplified urgency
 */
static brain_salience_t compute_salience_balanced(salience_evaluator_t eval, const float* features,
                                                  uint32_t num_features, uint64_t timestamp)
{
    brain_salience_t salience = {0};

    // Full novelty computation
    if (eval->config.enable_novelty) {
        salience.novelty = history_buffer_compute_novelty(eval->history, features, num_features);
    }

    // Full surprise computation
    if (eval->config.enable_surprise) {
        salience.surprise = predictor_compute_surprise(eval->predictor, features, num_features);
    }

    // Enhanced urgency (add variance to baseline)
    if (eval->config.enable_urgency) {
        /**
         * WHAT: Urgency based on input variance
         * WHY: Rapidly changing inputs suggest urgency
         * HOW: Measure feature variance, add to baseline
         */
        float variance = 0.0F;
        for (uint32_t i = 0; i < num_features; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_features > 256) {
                salience_heartbeat("salience_loop",
                                 (float)(i + 1) / (float)num_features);
            }

            variance += features[i] * features[i];
        }
        if (num_features > 0) { variance /= num_features; }

        float urgency_boost = fminf(0.3F, variance);
        salience.urgency = fminf(1.0F, eval->config.urgency_baseline + urgency_boost);
    }

    // Combine scores
    float total_weight =
        eval->config.novelty_weight + eval->config.surprise_weight + eval->config.urgency_weight;

    if (total_weight > 0.0F) {
        salience.salience = (salience.novelty * eval->config.novelty_weight +
                             salience.surprise * eval->config.surprise_weight +
                             salience.urgency * eval->config.urgency_weight) /
                            total_weight;
    }

    salience.confidence = 0.85F;
    salience.estimated_cost = 0.5F;

    return salience;
}


/**
 * WHAT: Accurate deep salience computation
 * WHY: Maximum accuracy for critical decisions
 * HOW: Full computation including partial brain activation
 *
 * PARTIAL BRAIN ACTIVATION:
 * - Activate early layers to get feature statistics
 * - Compute intensity, contrast, and extremity salience
 * - Optionally sample full brain for high-salience inputs
 */
static brain_salience_t compute_salience_accurate(salience_evaluator_t eval, const float* features,
                                                  uint32_t num_features, uint64_t timestamp)
{
    (void)timestamp;
    brain_salience_t salience = {0};

    // Novelty and surprise from balanced mode
    if (eval->config.enable_novelty && eval->history) {
        salience.novelty = history_buffer_compute_novelty(eval->history, features, num_features);
    }
    if (eval->config.enable_surprise && eval->predictor) {
        salience.surprise = predictor_compute_surprise(eval->predictor, features, num_features);
    }

    // PARTIAL BRAIN ACTIVATION
    float activation_salience = 0.0f;
    float activation_urgency = eval->config.urgency_baseline;

    if (eval->brain) {
        uint32_t num_inputs = brain_get_num_inputs(eval->brain);
        uint32_t safe_features = (num_features < num_inputs) ? num_features : num_inputs;

        // Feature statistics for activation estimation
        float feature_mean = 0.0f, feature_max = 0.0f, feature_variance = 0.0f;
        for (uint32_t i = 0; i < safe_features; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && safe_features > 256) {
                salience_heartbeat("salience_loop",
                                 (float)(i + 1) / (float)safe_features);
            }

            feature_mean += features[i];
            if (fabsf(features[i]) > fabsf(feature_max)) feature_max = features[i];
        }
        if (safe_features > 0) { feature_mean /= safe_features; }

        for (uint32_t i = 0; i < safe_features; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && safe_features > 256) {
                salience_heartbeat("salience_loop",
                                 (float)(i + 1) / (float)safe_features);
            }

            float diff = features[i] - feature_mean;
            feature_variance += diff * diff;
        }
        if (safe_features > 0) { feature_variance /= safe_features; }
        float feature_std = sqrtf(feature_variance);

        // Salience from activation patterns
        float intensity_salience = fminf(1.0f, fabsf(feature_max));
        float contrast_salience = fminf(1.0f, feature_std * 2.0f);
        float extremity_salience = fminf(1.0f, fabsf(feature_mean) * 1.5f);

        activation_salience = 0.4f * contrast_salience + 0.35f * intensity_salience +
                              0.25f * extremity_salience;

        // Urgency from feature dynamics
        if (feature_variance > 0.2f) activation_urgency += fminf(0.3f, feature_variance - 0.2f);
        if (fabsf(feature_max) > 0.7f) activation_urgency += fminf(0.2f, (fabsf(feature_max) - 0.7f) * 0.67f);
        activation_urgency = fminf(1.0f, activation_urgency);
    }

    if (eval->config.enable_urgency) salience.urgency = activation_urgency;

    // Combine scores
    float total_weight = eval->config.novelty_weight + eval->config.surprise_weight +
                         eval->config.urgency_weight;
    if (total_weight > 0.0F) {
        float base_salience = (salience.novelty * eval->config.novelty_weight +
                               salience.surprise * eval->config.surprise_weight +
                               salience.urgency * eval->config.urgency_weight) / total_weight;
        salience.salience = 0.7f * base_salience + 0.3f * activation_salience;
    } else {
        salience.salience = activation_salience;
    }

    salience.confidence = 0.95F;
    salience.estimated_cost = 0.9F;
    return salience;
}


/**
 * @brief Apply acetylcholine gating to salience scores
 *
 * WHAT: Modulate all salience scores by current acetylcholine level
 * WHY:  ACh gates attention - models attentiveness vs drowsiness
 * HOW:  Read ACh from brain, compute modulation factor, scale all scores
 *
 * BIOLOGY: Acetylcholine enhances sensory processing and attention
 *          High ACh (0.7) → 1.4× salience (highly attentive, focused)
 *          Low ACh (0.3) → 0.6× salience (inattentive, drowsy)
 *
 * COMPLEXITY: O(1)
 *
 * @param salience Salience scores to modulate (modified in-place)
 * @param brain Brain to read ACh from
 */
static void apply_acetylcholine_gating(brain_salience_t* salience, brain_t brain)
{
    // Guard: Early return if no brain
    if (!brain || !salience) {
        return;
    }

    // Get neuromodulator system
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    if (!neuromod) {
        return;
    }

    // Read current acetylcholine level
    float ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);

    // Map ACh range [0.3, 0.7] to modulation [0.6, 1.4]
    float modulation = 0.6F + (ach - 0.3F) * 2.0F;

    // Modulate all salience scores
    salience->salience *= modulation;
    salience->novelty *= modulation;
    salience->surprise *= modulation;
    salience->urgency *= modulation;

    // Clamp to [0, 1] range
    salience->salience = fminf(salience->salience, 1.0F);
    salience->novelty = fminf(salience->novelty, 1.0F);
    salience->surprise = fminf(salience->surprise, 1.0F);
    salience->urgency = fminf(salience->urgency, 1.0F);
}

static nimcp_error_t handle_salience_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_salience_query_t* query = (const bio_msg_salience_query_t*)msg;
    salience_evaluator_t eval = (salience_evaluator_t)user_data;

    LOG_DEBUG("Received salience query: stimulus=%u, intensity=%.2f, novelty=%.2f, relevance=%.2f",
              query->stimulus_id, query->raw_intensity, query->novelty, query->relevance);

    // Construct feature vector from query parameters
    // For simple queries, we use intensity, novelty, relevance as base features
    float features[3] = {
        query->raw_intensity,
        query->novelty,
        query->relevance
    };

    // Evaluate salience using configured strategy
    brain_salience_t salience = brain_evaluate_salience(eval, features, 3);

    LOG_DEBUG("Evaluated salience: score=%.2f, novelty=%.2f, surprise=%.2f, urgency=%.2f",
              salience.salience, salience.novelty, salience.surprise, salience.urgency);

    // Send response via promise if provided
    if (response_promise) {
        bio_msg_salience_response_t response = {0};
        bio_msg_init_header(&response.header, BIO_MSG_SALIENCE_RESPONSE,
                            bio_module_context_get_id(eval->bio_ctx), 0, sizeof(response));
        response.stimulus_id = query->stimulus_id;
        response.salience_score = salience.salience;
        response.attention_priority = salience.urgency;
        response.requires_immediate_attention = (salience.salience > eval->config.high_salience_threshold);

        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));

        LOG_DEBUG("Sent salience response: score=%.2f, priority=%.2f, immediate=%d",
                  response.salience_score, response.attention_priority,
                  response.requires_immediate_attention);
    }

    // Also broadcast high salience events
    if (salience.salience > eval->config.high_salience_threshold) {
        bio_broadcast_salience_response(eval, &salience, query->stimulus_id);
    }

    return NIMCP_SUCCESS;
}


/**
 * @brief Broadcast salience evaluation result via bio-async
 */
static void bio_broadcast_salience_response(salience_evaluator_t eval,
                                            const brain_salience_t* salience,
                                            uint32_t stimulus_id) {
    if (!eval || !salience || !eval->bio_async_enabled || !eval->bio_ctx) {
        return;
    }

    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(eval->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = stimulus_id;
    msg.salience_score = salience->salience;
    msg.attention_priority = salience->urgency;
    msg.requires_immediate_attention = (salience->salience > 0.7F);

    bio_router_broadcast(eval->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast salience: %.2f (novelty=%.2f, surprise=%.2f)",
              salience->salience, salience->novelty, salience->surprise);
}

static void evaluate_single_task(void* arg)
{
    batch_task_t* task = (batch_task_t*) arg;
    if (!task || !task->eval || !task->features || !task->output) {
        return;
    }

    /**
     * WHAT: Compute salience WITHOUT state updates
     * WHY: Avoid lock contention - state updated after batch completes
     * HOW: Call strategy function directly, skip history/predictor updates
     */
    salience_evaluator_t eval = task->eval;

    /* WHAT: Use configured strategy for evaluation */
    switch (eval->config.strategy) {
        case SALIENCE_STRATEGY_FAST:
            *task->output = compute_salience_fast(eval, task->features, task->num_features, 0);
            break;

        case SALIENCE_STRATEGY_BALANCED:
            *task->output = compute_salience_balanced(eval, task->features, task->num_features, 0);
            break;

        case SALIENCE_STRATEGY_ACCURATE:
            *task->output = compute_salience_accurate(eval, task->features, task->num_features, 0);
            break;
    }
}
