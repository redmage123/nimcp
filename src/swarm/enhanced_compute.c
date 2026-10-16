/**
 * @file enhanced_compute.c
 * @brief Enhanced phi computation and metrics aggregation
 *
 * WHAT: Compute enhanced collective consciousness metrics with remote phi
 * WHY:  Single responsibility for phi collection, aggregation, and metrics
 * HOW:  Collect remote phi values and compute comprehensive metrics
 *
 * @author NIMCP Development Team
 * @date 2026-02-16
 * @version 2.6.3
 */

#include "nimcp_swarm_consciousness_enhanced_internal.h"

//=============================================================================
// Remote Phi Collection Functions
//=============================================================================

/**
 * WHAT: Request phi value from specific drone
 * WHY:  Collect remote consciousness values
 * HOW:  Send SWARM_MSG_PHI_REQUEST via signal adapter
 */
bool swarm_consciousness_request_phi(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_request_phi: ctx is NULL");
        return false;
    }

    if (!ctx->attached || !ctx->attached_swarm) {
        LOG_WARN("Not attached to swarm, cannot request phi");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_request_phi: required parameter is NULL (ctx->attached, ctx->attached_swarm)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Check if already pending
    for (uint32_t i = 0; i < ctx->pending_request_count; i++) {
        if (ctx->pending_requests[i].drone_id == drone_id &&
            !ctx->pending_requests[i].response_received) {
            nimcp_mutex_unlock(&ctx->lock);
            return true;  // Already pending
        }
    }

    // Add to pending requests
    if (ctx->pending_request_count < SWARM_CONSCIOUSNESS_MAX_PHI_REQUESTS) {
        phi_request_t* req = &ctx->pending_requests[ctx->pending_request_count++];
        req->drone_id = drone_id;
        req->request_time_ms = enhanced_get_time_ms();
        req->response_received = false;
        req->phi_value = 0.0f;
    }

    ctx->total_phi_requests++;

    nimcp_mutex_unlock(&ctx->lock);

    // Send request via swarm protocol
    // Message format: [msg_type][drone_id]
    uint8_t msg_buffer[3];
    msg_buffer[0] = SWARM_MSG_PHI_REQUEST;
    msg_buffer[1] = (uint8_t)(drone_id & 0xFF);
    msg_buffer[2] = (uint8_t)((drone_id >> 8) & 0xFF);

    if (ctx->signal_adapter) {
        swarm_signal_send(ctx->signal_adapter, msg_buffer, sizeof(msg_buffer), drone_id);
        LOG_DEBUG("Sent phi request to drone %u", drone_id);
        return true;
    }

    // No signal adapter configured - caller should set one via
    // swarm_consciousness_set_signal_adapter()
    LOG_DEBUG("No signal adapter configured for phi requests");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_request_phi: validation failed");
    return false;
}

/**
 * WHAT: Request phi from all known peers
 * WHY:  Full collective consciousness computation
 * HOW:  Iterate peers, send requests
 */
uint32_t swarm_consciousness_request_all_phi(
    swarm_consciousness_enhanced_ctx_t* ctx)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        return 0;
    }

    if (!ctx->attached || !ctx->attached_swarm) {
        return 0;
    }

    // Get peer list
    uint32_t peer_count = 0;
    const swarm_peer_info_t* peers = swarm_brain_get_peers(ctx->attached_swarm, &peer_count);
    if (!peers || peer_count == 0) {
        return 0;
    }

    uint32_t requests_sent = 0;
    for (uint32_t i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            if (swarm_consciousness_request_phi(ctx, peers[i].drone_id)) {
                requests_sent++;
            }
        }
    }

    LOG_INFO("Requested phi from %u peers", requests_sent);
    return requests_sent;
}

/**
 * WHAT: Handle incoming phi response
 * WHY:  Store remote phi values
 * HOW:  Update remote_phi array, invoke callback
 */
bool swarm_consciousness_handle_phi_response(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id,
    float phi_value)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_handle_phi_response: ctx is NULL");
        return false;
    }

    // Validate phi
    if (phi_value < 0.0f || phi_value > SWARM_CONSCIOUSNESS_MAX_PHI ||
        isnan(phi_value) || isinf(phi_value)) {
        LOG_WARN("Invalid phi value from drone %u: %.3f", drone_id, phi_value);
        bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "invalid_phi",
                      "drone=%u phi=%.3f", drone_id, phi_value);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_handle_phi_response: operation failed");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->total_phi_responses++;

    // Mark request as completed
    for (uint32_t i = 0; i < ctx->pending_request_count; i++) {
        if (ctx->pending_requests[i].drone_id == drone_id) {
            ctx->pending_requests[i].response_received = true;
            ctx->pending_requests[i].phi_value = phi_value;
            break;
        }
    }

    // Update or add to remote phi collection
    bool found = false;
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        if (ctx->remote_phi[i].drone_id == drone_id) {
            ctx->remote_phi[i].phi_value = phi_value;
            ctx->remote_phi[i].timestamp_ms = enhanced_get_time_ms();
            ctx->remote_phi[i].valid = true;
            found = true;
            break;
        }
    }

    if (!found && ctx->remote_phi_count < SWARM_CONSCIOUSNESS_MAX_DRONES) {
        remote_phi_entry_t* entry = &ctx->remote_phi[ctx->remote_phi_count++];
        entry->drone_id = drone_id;
        entry->phi_value = phi_value;
        entry->timestamp_ms = enhanced_get_time_ms();
        entry->valid = true;
    }

    nimcp_mutex_unlock(&ctx->lock);

    // Invoke callback
    enhanced_invoke_peer_callback(ctx, PEER_EVENT_PHI_UPDATE, drone_id, phi_value,
                         ctx->remote_phi_count + 1);

    LOG_DEBUG("Received phi from drone %u: %.3f", drone_id, phi_value);

    return true;
}

/**
 * WHAT: Get collected remote phi values
 * WHY:  Access remote phis for computation
 * HOW:  Copy from internal storage
 */
bool swarm_consciousness_get_remote_phi(
    const swarm_consciousness_enhanced_ctx_t* ctx,
    float* phi_values,
    uint16_t* drone_ids,
    uint32_t* count)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC ||
        !phi_values || !drone_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_get_remote_phi: operation failed");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&ctx->lock);

    *count = ctx->remote_phi_count;
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        phi_values[i] = ctx->remote_phi[i].phi_value;
        drone_ids[i] = ctx->remote_phi[i].drone_id;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)&ctx->lock);

    return true;
}

//=============================================================================
// Enhanced Computation Functions
//=============================================================================

/**
 * WHAT: Compute enhanced collective consciousness metrics
 * WHY:  Full analysis including geometry, dynamics, binding
 * HOW:  Aggregate base + all enhanced metrics
 */
swarm_consciousness_enhanced_metrics_t* swarm_compute_enhanced_metrics(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC || !swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_enhanced_metrics: required parameter is NULL (ctx, swarm)");
        return NULL;
    }

    return enhanced_compute_metrics_impl(ctx, swarm);
}

/**
 * WHAT: Internal implementation of enhanced metrics computation
 * WHY:  Separate public API from implementation
 * HOW:  Compute all metric subsystems
 */
swarm_consciousness_enhanced_metrics_t* enhanced_compute_metrics_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm)
{
    // Allocate result
    swarm_consciousness_enhanced_metrics_t* metrics =
        (swarm_consciousness_enhanced_metrics_t*)nimcp_calloc(1, sizeof(*metrics));
    if (!metrics) {
        LOG_ERROR("Failed to allocate enhanced metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "enhanced_compute_metrics_impl: metrics is NULL");
        return NULL;
    }

    // Compute base metrics
    swarm_consciousness_metrics_t* base = swarm_compute_collective_phi(swarm, &ctx->config.base);
    if (base) {
        memcpy(&metrics->base, base, sizeof(swarm_consciousness_metrics_t));
        swarm_consciousness_metrics_free(base);
    }

    nimcp_mutex_lock(&ctx->lock);

    // Update phi with remote values
    float total_phi = ctx->local_phi;
    uint32_t total_count = 1;
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        if (ctx->remote_phi[i].valid) {
            total_phi += ctx->remote_phi[i].phi_value;
            total_count++;
        }
    }

    // Override base collective phi with true collective value
    if (total_count > 1) {
        metrics->base.collective_phi = total_phi *
            (1.0f + ctx->config.base.integration_weight * metrics->base.network_integration);
        metrics->base.drone_count = total_count;
    }

    // Remote phi collection status
    metrics->remote_phi_collected = ctx->remote_phi_count;
    metrics->remote_phi_pending = ctx->pending_request_count;
    metrics->collection_complete = (ctx->pending_request_count == 0);

    // Add to history
    enhanced_add_phi_to_history(ctx, metrics->base.collective_phi);

    // Copy history
    metrics->history_count = ctx->history_count;
    metrics->history_index = ctx->history_index;
    memcpy(metrics->phi_history, ctx->phi_history, sizeof(ctx->phi_history));

    nimcp_mutex_unlock(&ctx->lock);

    // Compute advanced metrics if enabled
    if (ctx->config.enable_geometry && ctx->history_count >= MIN_GEOMETRY_SAMPLES) {
        enhanced_compute_geometry_impl(ctx, &metrics->geometry);
    }

    if (ctx->config.enable_dynamics && ctx->history_count >= MIN_DYNAMICS_SAMPLES) {
        enhanced_compute_dynamics_impl(ctx, &metrics->dynamics);

        // Check for phase transition
        bool transition_detected = false;
        consciousness_phase_t new_phase = swarm_consciousness_detect_phase_transition(
            ctx, &transition_detected);
        if (transition_detected) {
            enhanced_invoke_phase_callback(ctx, ctx->current_phase, new_phase, metrics);
            nimcp_mutex_lock(&ctx->lock);
            ctx->current_phase = new_phase;
            ctx->phase_transitions++;
            nimcp_mutex_unlock(&ctx->lock);
        }
    }

    if (ctx->config.enable_binding) {
        enhanced_compute_binding_impl(ctx, &metrics->binding);

        // Check for binding event
        if (metrics->binding.binding_active && !ctx->binding_active) {
            enhanced_invoke_binding_callback(ctx, &metrics->binding);
            nimcp_mutex_lock(&ctx->lock);
            ctx->binding_active = true;
            ctx->binding_events++;
            nimcp_mutex_unlock(&ctx->lock);
        } else if (!metrics->binding.binding_active && ctx->binding_active) {
            nimcp_mutex_lock(&ctx->lock);
            ctx->binding_active = false;
            nimcp_mutex_unlock(&ctx->lock);
        }
    }

    if (ctx->config.enable_hierarchy) {
        enhanced_compute_hierarchy_impl(ctx, swarm, &metrics->hierarchy);
    }

    if (ctx->config.enable_resilience) {
        enhanced_compute_resilience_impl(ctx, swarm, &metrics->resilience);
    }

    LOG_DEBUG("Enhanced metrics computed: phi=%.3f, geometry=%d, dynamics=%d, binding=%d",
              metrics->base.collective_phi,
              ctx->config.enable_geometry,
              ctx->config.enable_dynamics,
              ctx->config.enable_binding);

    return metrics;
}
