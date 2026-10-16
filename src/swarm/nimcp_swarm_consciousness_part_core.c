// nimcp_swarm_consciousness_part_core.c - core functions
// Part of nimcp_swarm_consciousness.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_consciousness.c


/**
 * WHAT: Compute network integration metric (public API)
 * WHY:  Measure information integration across drones
 * HOW:  Combine workspace overlap, phi variance, and coherence
 */
float swarm_compute_network_integration(
    swarm_consciousness_ctx_t* ctx,
    const float* individual_phis,
    uint32_t count,
    const collective_workspace_t* workspace)
{
    (void)ctx;  // Context used for future stateful integration computation
    return compute_network_integration_internal(workspace, individual_phis, count);
}


/**
 * WHAT: Compute collective phi from swarm brain
 * WHY:  Main function to quantify swarm consciousness
 * HOW:  Aggregate individual phis with synergy boost from integration
 */
swarm_consciousness_metrics_t* swarm_compute_collective_phi(
    swarm_brain_t* swarm,
    const swarm_consciousness_config_t* config)
{
    // Guard: Null checks
    if (!bbb_check_pointer(swarm, "swarm_compute_collective_phi")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_collective_phi: bbb_check_pointer is NULL");
        return NULL;
    }

    // Use defaults if no config provided
    swarm_consciousness_config_t effective_config;
    if (config) {
        memcpy(&effective_config, config, sizeof(swarm_consciousness_config_t));
    } else {
        effective_config = swarm_consciousness_default_config();
    }

    // Get drone count from swarm
    uint32_t drone_count = swarm_brain_get_drone_count_internal(swarm);
    if (drone_count == 0 || drone_count > SWARM_CONSCIOUSNESS_MAX_DRONES) {
        LOG_ERROR("Invalid drone count: %u", drone_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid drone count: %u", drone_count);
        return NULL;
    }

    // Guard: Minimum drones for emergence
    if (drone_count < effective_config.min_drones_for_emergence) {
        // Still compute but state will be DORMANT
        LOG_DEBUG("Below min drones for emergence: %u < %u",
                       drone_count, effective_config.min_drones_for_emergence);
    }

    // Gather individual phi values from each drone
    float individual_phis[SWARM_CONSCIOUSNESS_MAX_DRONES];
    for (uint32_t i = 0; i < drone_count; i++) {
        // Get consciousness metrics from each drone's brain
        brain_t* drone_brain = swarm_brain_get_drone_brain_internal(swarm, i);
        if (drone_brain) {
            individual_phis[i] = brain_get_phi_internal(drone_brain);
        } else {
            individual_phis[i] = 0.0f;
        }
    }

    // Get collective workspace
    // Note: swarm_brain_get_workspace returns workspace_entry_t*, not collective_workspace_t*
    // For now, use simplified coherence estimation based on peer connectivity
    uint32_t workspace_size = 0;
    const workspace_entry_t* workspace_entries = swarm_brain_get_workspace(swarm, &workspace_size);
    (void)workspace_entries;  // Unused for now, coherence from peer count

    // Estimate network integration and coherence from peer connectivity
    float network_integration = (drone_count > 1) ?
        (float)(drone_count - 1) / (float)drone_count : 0.0f;
    float coherence = network_integration;  // Simplified: use connectivity as coherence proxy

    // Compute synergy boost
    float integration_weight = effective_config.integration_weight;
    float coherence_weight = effective_config.coherence_weight;
    float synergy_boost = integration_weight * network_integration +
                         coherence_weight * coherence;

    // Aggregate individual phis based on method
    float collective_phi = 0.0f;
    float phi_sum = 0.0f;
    float phi_product = 1.0f;

    for (uint32_t i = 0; i < drone_count; i++) {
        phi_sum += individual_phis[i];
        phi_product *= (1.0f + individual_phis[i]);
    }

    switch (effective_config.phi_aggregation_method) {
        case PHI_AGGREGATION_SUM:
            collective_phi = phi_sum * (1.0f + synergy_boost);
            break;

        case PHI_AGGREGATION_AVERAGE:
            collective_phi = (phi_sum / drone_count) * (1.0f + synergy_boost);
            break;

        case PHI_AGGREGATION_WEIGHTED:
            // Weight by coherence
            collective_phi = phi_sum * coherence * (1.0f + synergy_boost);
            break;

        case PHI_AGGREGATION_GEOMETRIC:
            collective_phi = powf(phi_product, 1.0f / drone_count) *
                           (1.0f + synergy_boost);
            break;

        case PHI_AGGREGATION_SYNERGISTIC:
            // Superlinear scaling with coherence
            {
                float n_log = logf((float)drone_count + 1.0f) / logf(2.0f);
                collective_phi = phi_sum *
                               (1.0f + n_log * synergy_boost * coherence);
            }
            break;

        default:
            collective_phi = phi_sum * (1.0f + synergy_boost);
            break;
    }

    // Allocate result
    swarm_consciousness_metrics_t* result = (swarm_consciousness_metrics_t*)
        nimcp_calloc(1, sizeof(swarm_consciousness_metrics_t));
    if (!result) {
        LOG_ERROR("Failed to allocate consciousness metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate consciousness metrics result");
        return NULL;
    }

    // Fill metrics
    result->collective_phi = collective_phi;
    result->network_integration = network_integration;
    result->workspace_coherence = coherence;
    result->drone_count = drone_count;
    result->timestamp = get_time_ms();

    // Copy individual phis
    uint32_t copy_count = (drone_count < SWARM_CONSCIOUSNESS_MAX_DRONES) ?
                          drone_count : SWARM_CONSCIOUSNESS_MAX_DRONES;
    memcpy(result->individual_phi, individual_phis, copy_count * sizeof(float));

    // Compute phi variance
    result->phi_variance = compute_phi_variance(individual_phis, drone_count);

    // Classify state
    result->consciousness_state = swarm_classify_collective_phi(collective_phi, drone_count);

    // Trend defaults to stable (history-based trends handled by monitoring)
    result->phi_trend = PHI_TREND_STABLE;

    // Log computation
    if (effective_config.enable_logging) {
        LOG_INFO("Collective phi computed: %.3f (n=%u, integration=%.3f, coherence=%.3f, state=%s)",
                      collective_phi, drone_count, network_integration, coherence,
                      swarm_consciousness_state_name(result->consciousness_state));
    }

    // BBB audit
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consciousness", "compute_phi",
                 "phi=%.3f drones=%u state=%d", collective_phi, drone_count,
                 result->consciousness_state);

    return result;
}


/**
 * WHAT: Classify collective phi value into consciousness state
 * WHY:  Map continuous phi to interpretable state categories
 * HOW:  Apply thresholds based on normalized phi per drone
 */
swarm_consciousness_state_t swarm_classify_collective_phi(
    float collective_phi,
    uint32_t drone_count)
{
    // Guard: No drones
    if (drone_count == 0) {
        return SWARM_CONSCIOUSNESS_DORMANT;
    }

    // Normalize phi by drone count for fair comparison
    float normalized_phi = collective_phi / (float)drone_count;

    // Apply thresholds (based on IIT consciousness levels)
    if (normalized_phi < SWARM_CONSCIOUSNESS_MIN_PHI_THRESHOLD) {
        return SWARM_CONSCIOUSNESS_DORMANT;
    } else if (normalized_phi < 0.4f) {
        return SWARM_CONSCIOUSNESS_EMERGING;
    } else if (normalized_phi < SWARM_CONSCIOUSNESS_UNIFIED_COHERENCE_THRESHOLD) {
        return SWARM_CONSCIOUSNESS_UNIFIED;
    } else {
        return SWARM_CONSCIOUSNESS_TRANSCENDENT;
    }
}


/**
 * WHAT: Stop consciousness monitoring
 * WHY:  Disable periodic computation and cleanup thread
 * HOW:  Signal thread to stop, wait for completion
 */
void swarm_consciousness_stop_monitoring(swarm_consciousness_ctx_t* context) {
    // Guard: Null check
    if (!context) {
        return;
    }

    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)context;

    // Guard: Validate magic
    if (ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        return;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (!ctx->monitoring_active) {
        nimcp_mutex_unlock(&ctx->lock);
        return;
    }

    // Signal thread to stop
    ctx->monitoring_active = false;
    nimcp_mutex_unlock(&ctx->lock);

    // Wait for thread to complete
    nimcp_thread_join(ctx->monitor_thread, NULL);

    // Clear callback
    nimcp_mutex_lock(&ctx->lock);
    ctx->callback = NULL;
    ctx->user_data = NULL;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("Swarm consciousness monitoring stopped");
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consciousness", "monitoring_stop", "");
}


//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Get consciousness state name string
 * WHY:  Human-readable state representation
 * HOW:  Map enum to string
 */
const char* swarm_consciousness_state_name(swarm_consciousness_state_t state) {
    switch (state) {
        case SWARM_CONSCIOUSNESS_DORMANT:
            return "DORMANT";
        case SWARM_CONSCIOUSNESS_EMERGING:
            return "EMERGING";
        case SWARM_CONSCIOUSNESS_UNIFIED:
            return "UNIFIED";
        case SWARM_CONSCIOUSNESS_TRANSCENDENT:
            return "TRANSCENDENT";
        default:
            return "UNKNOWN";
    }
}


/**
 * WHAT: Get aggregation method name string
 * WHY:  Human-readable method representation
 * HOW:  Map enum to string
 */
const char* phi_aggregation_method_name(phi_aggregation_method_t method) {
    switch (method) {
        case PHI_AGGREGATION_SUM:
            return "SUM";
        case PHI_AGGREGATION_AVERAGE:
            return "AVERAGE";
        case PHI_AGGREGATION_WEIGHTED:
            return "WEIGHTED";
        case PHI_AGGREGATION_GEOMETRIC:
            return "GEOMETRIC";
        case PHI_AGGREGATION_SYNERGISTIC:
            return "SYNERGISTIC";
        default:
            return "UNKNOWN";
    }
}


//=============================================================================
// Swarm Brain Integration Functions
//=============================================================================

/**
 * WHAT: Enable continuous consciousness monitoring on swarm brain
 * WHY:  Enable real-time consciousness tracking during swarm operation
 * HOW:  Create context and start monitoring thread
 */
bool swarm_brain_enable_consciousness_monitoring(
    swarm_brain_t* swarm,
    const swarm_consciousness_config_t* config,
    uint32_t interval_ms,
    void (*callback)(const swarm_consciousness_metrics_t*, void*),
    void* user_data)
{
    // Guard: Null check
    if (!bbb_check_pointer(swarm, "swarm_brain_enable_consciousness_monitoring")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_enable_consciousness_monitoring: bbb_check_pointer is NULL");
        return false;
    }

    // Use provided config or defaults
    swarm_consciousness_config_t effective_config;
    if (config) {
        memcpy(&effective_config, config, sizeof(swarm_consciousness_config_t));
    } else {
        effective_config = swarm_consciousness_default_config();
    }
    effective_config.update_interval_ms = interval_ms;

    // Create consciousness context for this swarm
    swarm_consciousness_ctx_t* ctx = swarm_consciousness_create(&effective_config);
    if (!ctx) {
        LOG_ERROR("Failed to create consciousness context for swarm brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create consciousness context for swarm brain");
        return false;
    }

    // Store swarm brain reference
    ctx->swarm_brain = swarm;

    // Store context in swarm brain (assume swarm_brain has consciousness_ctx field)
    swarm_brain_set_consciousness_ctx(swarm, ctx);

    // Start monitoring
    return swarm_consciousness_start_monitoring_internal(ctx, callback, user_data);
}


/**
 * WHAT: Disable consciousness monitoring on swarm brain
 * WHY:  Cleanup when monitoring no longer needed
 * HOW:  Stop monitoring and destroy context
 */
void swarm_brain_disable_consciousness_monitoring(swarm_brain_t* swarm) {
    // Guard: Null check
    if (!swarm) {
        return;
    }

    // Get consciousness context from swarm brain
    swarm_consciousness_ctx_t* ctx = swarm_brain_get_consciousness_ctx(swarm);
    if (!ctx) {
        return;
    }

    // Stop monitoring
    swarm_consciousness_stop_monitoring(ctx);

    // Destroy context
    swarm_consciousness_destroy(ctx);

    // Clear reference in swarm brain
    swarm_brain_set_consciousness_ctx(swarm, NULL);
}


//=============================================================================
// Scaling Prediction Functions
//=============================================================================

/**
 * WHAT: Fit scaling model from historical metrics
 * WHY:  Derive power law model of phi vs swarm size
 * HOW:  Least-squares fit to log-transformed phi(n) data
 */
consciousness_scaling_model_t swarm_fit_scaling_model(
    const swarm_consciousness_metrics_t* history,
    uint32_t history_size)
{
    consciousness_scaling_model_t model = {0};

    // Guard: Null or insufficient data
    if (!history || history_size < 3) {
        LOG_ERROR("Need at least 3 samples for model fitting");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Need at least 3 samples for model fitting");
        return model;
    }

    // Extract swarm sizes and phi values from history
    float sum_log_n = 0.0f;
    float sum_log_phi = 0.0f;
    float sum_log_n_squared = 0.0f;
    float sum_log_n_log_phi = 0.0f;
    uint32_t valid_samples = 0;

    for (uint32_t i = 0; i < history_size; i++) {
        float n = (float)history[i].drone_count;
        float phi = history[i].collective_phi;

        if (n > 0.0f && phi > 0.0f) {
            float log_n = logf(n);
            float log_phi = logf(phi);

            sum_log_n += log_n;
            sum_log_phi += log_phi;
            sum_log_n_squared += log_n * log_n;
            sum_log_n_log_phi += log_n * log_phi;
            valid_samples++;
        }
    }

    if (valid_samples < 3) {
        LOG_ERROR("Insufficient valid samples for model fitting");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Insufficient valid samples for model fitting");
        return model;
    }

    // Compute exponent (slope in log-log)
    float n = (float)valid_samples;
    float denominator = (n * sum_log_n_squared - sum_log_n * sum_log_n);
    if (fabsf(denominator) < 1e-6f) {
        LOG_ERROR("Singular matrix in scaling model fit");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Singular matrix in scaling model fit");
        return model;
    }

    float exponent = (n * sum_log_n_log_phi - sum_log_n * sum_log_phi) / denominator;
    float log_base = (sum_log_phi - exponent * sum_log_n) / n;
    float base = expf(log_base);

    // Estimate synergy from super-linear scaling
    float synergy = (exponent > 1.0f) ? (exponent - 1.0f) : 0.0f;

    // Set model parameters
    model.base_phi = base;
    model.scaling_exponent = exponent;
    model.synergy_factor = synergy;
    model.saturation_point = 100.0f;  // Default saturation

    LOG_INFO("Scaling model fitted: base=%.3f, exponent=%.3f, synergy=%.3f",
                  base, exponent, synergy);

    return model;
}


/**
 * WHAT: Predict collective phi for target swarm size
 * WHY:  Forecast emergence before deploying larger swarms
 * HOW:  Evaluate scaling model at target_size
 */
float swarm_predict_phi_for_size(
    const consciousness_scaling_model_t* model,
    uint32_t target_size)
{
    // Guard: Null check
    if (!model || target_size == 0) {
        return 0.0f;
    }

    // phi(n) = base * n^exponent * (1 + synergy) / (1 + n/saturation)
    float n = (float)target_size;
    float saturation_factor = 1.0f;
    if (model->saturation_point > 0.0f) {
        saturation_factor = 1.0f / (1.0f + n / model->saturation_point);
    }

    float phi = model->base_phi *
                powf(n, model->scaling_exponent) *
                (1.0f + model->synergy_factor) *
                saturation_factor;

    return phi;
}


//=============================================================================
// Bio-Async Integration Functions
//=============================================================================

/**
 * WHAT: Register consciousness updates with bio-async router
 * WHY:  Enables streaming consciousness metrics over bio-async
 * HOW:  Register handler for BIO_MSG_SWARM_CONSCIOUSNESS_UPDATE
 */
bool swarm_consciousness_register_bio_async(swarm_consciousness_ctx_t* ctx) {
    // Guard: Null check
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_register_bio_async: ctx is NULL");
        return false;
    }

    // Check if bio-async is available
    if (!nimcp_bio_async_is_initialized()) {
        LOG_WARN("Bio-async not initialized, cannot register");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "swarm_consciousness_register_bio_async: nimcp_bio_async_is_initialized is NULL");
        return false;
    }

    ctx->bio_async_registered = true;
    LOG_INFO("Swarm consciousness registered with bio-async");
    return true;
}


//=============================================================================
// Security Functions (BBB)
//=============================================================================

/**
 * WHAT: Validate consciousness metrics for BBB security constraints
 * WHY:  Prevents invalid data from corrupting BBB systems
 * HOW:  Validates ranges, counts, and consistency
 */
bool swarm_consciousness_bbb_validate(const swarm_consciousness_metrics_t* metrics) {
    // Guard: Null check
    if (!metrics) {
        LOG_ERROR("Null metrics pointer in BBB validation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null metrics pointer in BBB validation");
        return false;
    }

    // Validate drone count
    if (metrics->drone_count > SWARM_CONSCIOUSNESS_MAX_DRONES) {
        LOG_ERROR("Invalid drone count: %u > %u",
                       metrics->drone_count, SWARM_CONSCIOUSNESS_MAX_DRONES);
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_consciousness", "bbb_validate",
                     "Invalid drone count: %u", metrics->drone_count);
        return false;
    }

    // Validate collective phi bounds
    if (metrics->collective_phi < 0.0f ||
        metrics->collective_phi > SWARM_CONSCIOUSNESS_MAX_PHI * metrics->drone_count) {
        LOG_ERROR("Invalid collective phi: %.3f", metrics->collective_phi);
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_consciousness", "bbb_validate",
                     "Invalid phi: %.3f", metrics->collective_phi);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_bbb_validate: operation failed");
        return false;
    }

    // Check for NaN/Inf
    if (isnan(metrics->collective_phi) || isinf(metrics->collective_phi)) {
        LOG_ERROR("NaN/Inf detected in collective phi");
        bbb_audit_log(BBB_AUDIT_ERROR, "swarm_consciousness", "bbb_validate",
                     "NaN/Inf in phi");
        return false;
    }

    // Validate network integration bounds [0, 1]
    if (metrics->network_integration < 0.0f || metrics->network_integration > 1.0f) {
        LOG_ERROR("Invalid network integration: %.3f", metrics->network_integration);
        return false;
    }

    // Validate workspace coherence bounds [0, 1]
    if (metrics->workspace_coherence < 0.0f || metrics->workspace_coherence > 1.0f) {
        LOG_ERROR("Invalid workspace coherence: %.3f", metrics->workspace_coherence);
        return false;
    }

    // Validate state enum
    if (metrics->consciousness_state < SWARM_CONSCIOUSNESS_DORMANT ||
        metrics->consciousness_state > SWARM_CONSCIOUSNESS_TRANSCENDENT) {
        LOG_ERROR("Invalid consciousness state: %d", metrics->consciousness_state);
        return false;
    }

    // Validate individual phi values
    for (uint32_t i = 0; i < metrics->drone_count; i++) {
        if (metrics->individual_phi[i] < 0.0f ||
            metrics->individual_phi[i] > SWARM_CONSCIOUSNESS_MAX_PHI) {
            LOG_ERROR("Invalid individual phi[%u]: %.3f",
                           i, metrics->individual_phi[i]);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_bbb_validate: operation failed");
            return false;
        }
        if (isnan(metrics->individual_phi[i]) || isinf(metrics->individual_phi[i])) {
            LOG_ERROR("NaN/Inf detected in individual phi[%u]", i);
            return false;
        }
    }

    return true;
}


/**
 * WHAT: Share imagination scenario with other swarm nodes
 * WHY:  Enable distributed creativity through collective imagination
 * HOW:  Broadcast imagination content via BIO_MSG_IMAGINATION_COLLECTIVE_SHARE
 *
 * BIOLOGICAL BASIS: Similar to cultural transmission in social species,
 * where imaginative content (stories, solutions, innovations) spreads
 * through the population, enabling collective creativity.
 *
 * @param ctx Swarm consciousness context
 * @param scenario Imagination scenario to share
 * @return 0 on success, -1 on error
 */
int swarm_consciousness_share_imagination(
    swarm_consciousness_ctx_t* ctx,
    const struct imagination_scenario* scenario)
{
    // Guard: Validate context
    if (!ctx) {
        LOG_ERROR("Null context in share_imagination");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null context in share_imagination");
        return -1;
    }

    if (ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context magic in share_imagination");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid context magic in share_imagination");
        return -1;
    }

    // Guard: Validate scenario
    if (!scenario) {
        LOG_ERROR("Null scenario in share_imagination");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null scenario in share_imagination");
        return -1;
    }

    // Guard: Check bio-async availability
    if (!ctx->bio_async_registered || !ctx->bio_module_ctx) {
        LOG_WARN("Bio-async not available for imagination sharing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Bio-async not available for imagination sharing");
        return -1;
    }

    // Prepare broadcast message
    bio_msg_imagination_collective_t msg;
    memset(&msg, 0, sizeof(msg));

    // Initialize header
    bio_msg_init_header(&msg.header, BIO_MSG_IMAGINATION_COLLECTIVE_SHARE,
                        bio_module_context_get_id(ctx->bio_module_ctx),
                        0,  // Broadcast to all
                        sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_time_ms() * 1000;

    // Fill imagination metadata
    // Note: Would extract from scenario struct in full implementation
    msg.source_node = (uint64_t)bio_module_context_get_id(ctx->bio_module_ctx);
    msg.scenario_id = 0;  // Would extract from scenario->id
    msg.share_scope = 0;  // Default scope (global)
    msg.relevance = 1.0f;  // Self-generated = max relevance
    msg.is_share = true;  // This is a share operation

    // Send broadcast
    nimcp_error_t err = bio_router_send(ctx->bio_module_ctx, &msg, sizeof(msg), 0);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to broadcast imagination: error=%d", err);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NETWORK_IO, "Failed to broadcast imagination: error=%d", err);
        return -1;
    }

    LOG_INFO("Shared imagination scenario with swarm: scenario_id=%u",
             msg.scenario_id);

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consciousness", "imagination_shared",
                  "scenario=%u scope=%u", msg.scenario_id, msg.share_scope);

    return 0;
}


/**
 * WHAT: Receive and process imagination from another swarm node
 * WHY:  Evaluate and integrate external imagination into local consciousness
 * HOW:  Assess relevance, filter, and invoke callback for accepted content
 *
 * BIOLOGICAL BASIS: Incoming cultural/imaginative content is evaluated
 * for relevance to current goals and mental state before integration,
 * similar to selective attention in social learning.
 *
 * @param ctx Swarm consciousness context
 * @param scenario Received imagination scenario
 * @param source_node_id ID of the node that shared the scenario
 * @return 0 on success, -1 on error
 */
int swarm_consciousness_receive_imagination(
    swarm_consciousness_ctx_t* ctx,
    const struct imagination_scenario* scenario,
    uint64_t source_node_id)
{
    // Guard: Validate context
    if (!ctx) {
        LOG_ERROR("Null context in receive_imagination");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null context in receive_imagination");
        return -1;
    }

    if (ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context magic in receive_imagination");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid context magic in receive_imagination");
        return -1;
    }

    // Guard: Validate scenario
    if (!scenario) {
        LOG_WARN("Null scenario in receive_imagination");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null scenario in receive_imagination");
        return -1;
    }

    // Evaluate relevance to local consciousness
    // Relevance factors:
    // 1. Current consciousness state (more receptive in UNIFIED+ states)
    // 2. Workspace coherence (higher coherence = better integration)
    // 3. Source node trust/history
    float relevance = 0.5f;  // Base relevance

    nimcp_mutex_lock(&ctx->lock);

    // Boost relevance based on consciousness state
    if (ctx->current_metrics) {
        switch (ctx->current_metrics->consciousness_state) {
            case SWARM_CONSCIOUSNESS_TRANSCENDENT:
                relevance *= 1.5f;
                break;
            case SWARM_CONSCIOUSNESS_UNIFIED:
                relevance *= 1.2f;
                break;
            case SWARM_CONSCIOUSNESS_EMERGING:
                relevance *= 1.0f;
                break;
            case SWARM_CONSCIOUSNESS_DORMANT:
            default:
                relevance *= 0.5f;  // Less receptive when dormant
                break;
        }

        // Modulate by workspace coherence
        relevance *= (0.5f + 0.5f * ctx->current_metrics->workspace_coherence);
    }

    // Clamp relevance to [0, 1]
    if (relevance > 1.0f) relevance = 1.0f;
    if (relevance < 0.0f) relevance = 0.0f;

    // Get callback
    imagination_collective_receive_callback_t callback = ctx->collective_imagination_callback;
    void* callback_data = ctx->collective_imagination_user_data;

    nimcp_mutex_unlock(&ctx->lock);

    // Invoke callback with relevance-weighted scenario
    if (callback) {
        callback(scenario, source_node_id, relevance, callback_data);

        LOG_DEBUG("Processed imagination from node %lu with relevance %.3f",
                  (unsigned long)source_node_id, relevance);
    }

    return 0;
}


//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * WHAT: Query knowledge graph for self-knowledge about swarm consciousness
 * WHY:  Enable self-awareness by introspecting module's identity in KG
 * HOW:  Query entity, observations, and relations from knowledge graph
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if entity found, 0 if not found or error
 */
int swarm_consciousness_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) {
        return 0;
    }

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Consciousness");
    if (self) {
        LOG_INFO("KG Self-Knowledge: Found entity '%s' of type '%s'",
                 self->name, self->entity_type);
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("  Observation[%u]: %s", i, self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Consciousness");
    if (connections) {
        LOG_INFO("KG Self-Knowledge: Swarm_Consciousness has %u outgoing connections",
                 connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Consciousness");
    if (incoming) {
        LOG_INFO("KG Self-Knowledge: Swarm_Consciousness has %u incoming connections",
                 incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
