// nimcp_swarm_consciousness_part_helpers.c - helpers functions
// Part of nimcp_swarm_consciousness.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_consciousness.c


//=============================================================================
// Core Computation API
//=============================================================================

/**
 * WHAT: Compute network integration metric (internal helper)
 * WHY:  Measure information integration across drones
 * HOW:  Combine workspace overlap, phi variance, and coherence
 */
static float compute_network_integration_internal(
    const collective_workspace_t* workspace,
    const float* individual_phis,
    uint32_t drone_count)
{
    // Guard: Null checks
    if (!workspace || !individual_phis || drone_count == 0) {
        return 0.0f;
    }

    float integration = 0.0f;

    // 1. Workspace item overlap (shared information)
    float item_overlap = compute_workspace_overlap(workspace);

    // 2. Phi uniformity (low variance = more integrated)
    float phi_variance = compute_phi_variance(individual_phis, drone_count);
    float max_phi = 0.0f;
    for (uint32_t i = 0; i < drone_count; i++) {
        if (individual_phis[i] > max_phi) {
            max_phi = individual_phis[i];
        }
    }
    float phi_uniformity = (max_phi > 0.0f) ? (1.0f - (phi_variance / max_phi)) : 0.0f;

    // 3. Get workspace coherence
    float coherence = collective_workspace_get_coherence(workspace);

    // Combine metrics
    integration = 0.5f * item_overlap + 0.5f * phi_uniformity;
    integration *= coherence;

    // Clamp to [0, 1]
    if (integration < 0.0f) integration = 0.0f;
    if (integration > 1.0f) integration = 1.0f;

    return integration;
}



//=============================================================================
// Monitoring API
//=============================================================================

/**
 * WHAT: Monitoring thread function
 * WHY:  Periodic consciousness computation and callbacks
 * HOW:  Loop: compute phi → callback → publish → sleep
 */
static void* consciousness_monitor_thread(void* arg) {
    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)arg;

    LOG_INFO("Swarm consciousness monitoring thread started");

    while (ctx->monitoring_active) {
        // Note: In real implementation, would need workspace and individual_phis
        // This is a placeholder showing the pattern

        // Invoke callback if set
        if (ctx->callback && ctx->current_metrics) {
            nimcp_mutex_lock(&ctx->lock);
            swarm_consciousness_metrics_t metrics_copy;
            memcpy(&metrics_copy, ctx->current_metrics,
                   sizeof(swarm_consciousness_metrics_t));
            nimcp_mutex_unlock(&ctx->lock);

            ctx->callback(&metrics_copy, ctx->user_data);
        }

        // Publish via bio-async if enabled
        if (ctx->config.enable_bio_async && ctx->bio_async_registered) {
            publish_consciousness_update(ctx, ctx->current_metrics);
        }

        // Sleep for update interval
        usleep(ctx->config.update_interval_ms * 1000);
    }

    LOG_INFO("Swarm consciousness monitoring thread stopped");
    return NULL;
}


/**
 * WHAT: Start consciousness monitoring (internal)
 * WHY:  Enable periodic consciousness computation
 * HOW:  Spawn monitoring thread with callback
 */
static bool swarm_consciousness_start_monitoring_internal(
    swarm_consciousness_ctx_t* context,
    void (*callback)(const swarm_consciousness_metrics_t*, void*),
    void* user_data)
{
    // Guard: Null check
    if (!bbb_check_pointer(context, "swarm_consciousness_start_monitoring")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_start_monitoring_internal: bbb_check_pointer is NULL");
        return false;
    }

    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)context;

    // Guard: Validate magic
    if (ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid context magic in swarm_consciousness_start_monitoring");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Guard: Already monitoring
    if (ctx->monitoring_active) {
        LOG_WARN("Monitoring already active");
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_start_monitoring_internal: validation failed");
        return false;
    }

    // Set callback
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->monitoring_active = true;

    // Spawn monitoring thread
    if (nimcp_thread_create(&ctx->monitor_thread, consciousness_monitor_thread, ctx, NULL) != 0) {
        LOG_ERROR("Failed to create monitoring thread");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create consciousness monitoring thread");
        ctx->monitoring_active = false;
        ctx->callback = NULL;
        ctx->user_data = NULL;
        nimcp_mutex_unlock(&ctx->lock);
        return false;
    }

    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("Swarm consciousness monitoring started");
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consciousness", "monitoring_start",
                 "interval_ms=%u", ctx->config.update_interval_ms);

    return true;
}


//=============================================================================
// Helper Function Implementations
//=============================================================================

/**
 * WHAT: Compute workspace overlap metric
 * WHY:  Measure shared information across drones
 * HOW:  Analyze top workspace items for similarity
 */
static float compute_workspace_overlap(const collective_workspace_t* workspace) {
    if (!workspace) {
        return 0.0f;
    }

    // Get item count
    uint32_t item_count = collective_workspace_get_item_count(workspace);
    if (item_count == 0) {
        return 0.0f;
    }

    // Get coherence as proxy for overlap
    return collective_workspace_get_coherence(workspace);
}


/**
 * WHAT: Compute variance in individual phi values
 * WHY:  Measure uniformity of consciousness across drones
 * HOW:  Standard variance calculation
 */
static float compute_phi_variance(const float* individual_phis, uint32_t drone_count) {
    if (!individual_phis || drone_count == 0) {
        return 0.0f;
    }

    // Compute mean
    float mean = 0.0f;
    for (uint32_t i = 0; i < drone_count; i++) {
        mean += individual_phis[i];
    }
    mean /= drone_count;

    // Compute variance
    float variance = 0.0f;
    for (uint32_t i = 0; i < drone_count; i++) {
        float diff = individual_phis[i] - mean;
        variance += diff * diff;
    }
    variance /= drone_count;

    return variance;
}


/**
 * WHAT: Update phi history with new value
 * WHY:  Track consciousness trends over time
 * HOW:  Circular buffer insertion
 */
static void update_phi_history(swarm_consciousness_ctx_t* ctx, float phi) {
    if (!ctx) {
        return;
    }

    ctx->phi_history[ctx->history_index] = phi;
    ctx->history_index = (ctx->history_index + 1) % PHI_HISTORY_SIZE;

    if (ctx->history_count < PHI_HISTORY_SIZE) {
        ctx->history_count++;
    }
}


/**
 * WHAT: Compute phi trend from history
 * WHY:  Detect increasing/decreasing consciousness
 * HOW:  Simple linear regression slope
 */
static float get_phi_trend(const swarm_consciousness_ctx_t* ctx) {
    if (!ctx || ctx->history_count < 2) {
        return 0.0f;
    }

    // Simple slope estimation
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_xy = 0.0f;
    float sum_xx = 0.0f;

    uint32_t count = ctx->history_count;
    for (uint32_t i = 0; i < count; i++) {
        float x = (float)i;
        float y = ctx->phi_history[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    float n = (float)count;
    float denominator = (n * sum_xx - sum_x * sum_x);
    if (fabsf(denominator) < 1e-6f) {
        return 0.0f;
    }

    float slope = (n * sum_xy - sum_x * sum_y) / denominator;
    return slope;
}


/**
 * WHAT: Get current time in milliseconds
 * WHY:  Timestamp consciousness measurements
 * HOW:  Use clock_gettime for precision
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
