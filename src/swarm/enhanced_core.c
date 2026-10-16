/**
 * @file enhanced_core.c
 * @brief Core lifecycle, callbacks, and integration for enhanced consciousness
 *
 * WHAT: Context management, callbacks, swarm/bio-async integration
 * WHY:  Single responsibility for lifecycle and system integration
 * HOW:  Create/destroy, register callbacks, attach to swarm, handle messages
 *
 * @author NIMCP Development Team
 * @date 2026-02-16
 * @version 2.6.3
 */

#include "nimcp_swarm_consciousness_enhanced_internal.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_consciousness_enhanced)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Add phi value to history
 * WHY:  Track phi over time for dynamics analysis
 * HOW:  Circular buffer with timestamp
 */
void enhanced_add_phi_to_history(swarm_consciousness_enhanced_ctx_t* ctx, float phi) {
    ctx->phi_history[ctx->history_index] = phi;
    ctx->phi_timestamps[ctx->history_index] = (float)enhanced_get_time_ms();
    ctx->history_index = (ctx->history_index + 1) % SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE;
    if (ctx->history_count < SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE) {
        ctx->history_count++;
    }
}

/**
 * WHAT: Invoke peer event callback
 * WHY:  Notify application of peer changes
 * HOW:  Call registered callback with event data
 */
void enhanced_invoke_peer_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                  peer_event_type_t type, uint16_t drone_id,
                                  float phi, uint32_t new_count) {
    nimcp_mutex_lock(&ctx->lock);
    peer_event_callback_t callback = ctx->peer_callback;
    void* user_data = ctx->peer_callback_data;
    nimcp_mutex_unlock(&ctx->lock);

    if (callback) {
        peer_event_t event = {
            .event_type = type,
            .drone_id = drone_id,
            .phi_value = phi,
            .timestamp_ms = enhanced_get_time_ms(),
            .new_drone_count = new_count
        };
        callback(&event, user_data);
    }
}

/**
 * WHAT: Invoke phase transition callback
 * WHY:  Notify application of consciousness phase changes
 * HOW:  Call registered callback and publish via bio-async
 */
void enhanced_invoke_phase_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                   consciousness_phase_t old_phase,
                                   consciousness_phase_t new_phase,
                                   const swarm_consciousness_enhanced_metrics_t* metrics) {
    nimcp_mutex_lock(&ctx->lock);
    phase_transition_callback_t callback = ctx->phase_callback;
    void* user_data = ctx->phase_callback_data;
    nimcp_mutex_unlock(&ctx->lock);

    if (callback) {
        callback(old_phase, new_phase, metrics, user_data);
    }

    // Publish via bio-async
    if (ctx->bio_async_registered) {
        swarm_consciousness_publish_phase_transition(ctx, old_phase, new_phase);
    }
}

/**
 * WHAT: Invoke binding event callback
 * WHY:  Notify application of neural binding changes
 * HOW:  Call registered callback and publish via bio-async
 */
void enhanced_invoke_binding_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                     const neural_binding_t* binding) {
    nimcp_mutex_lock(&ctx->lock);
    binding_event_callback_t callback = ctx->binding_callback;
    void* user_data = ctx->binding_callback_data;
    nimcp_mutex_unlock(&ctx->lock);

    if (callback) {
        callback(binding, user_data);
    }

    // Publish via bio-async
    if (ctx->bio_async_registered) {
        swarm_consciousness_publish_binding_event(ctx, binding);
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * WHAT: Get default enhanced consciousness configuration
 * WHY:  Provide sensible defaults for all features
 * HOW:  Initialize struct with validated defaults
 */
swarm_consciousness_enhanced_config_t swarm_consciousness_enhanced_default_config(void) {
    swarm_consciousness_enhanced_config_t config = {0};

    // Base configuration
    config.base = swarm_consciousness_default_config();

    // Peer event settings
    config.enable_peer_callbacks = true;
    config.auto_collect_phi_on_join = true;
    config.phi_collection_timeout_ms = SWARM_CONSCIOUSNESS_PHI_REQUEST_TIMEOUT_MS;

    // Information geometry settings
    config.enable_geometry = true;
    config.geometry_history_size = 50;
    config.entropy_bin_width = 0.1f;

    // Dynamics settings
    config.enable_dynamics = true;
    config.dynamics_window_size = 100;
    config.critical_variance_threshold = 0.1f;

    // Neural binding settings
    config.enable_binding = true;
    config.gamma_frequency_hz = SWARM_CONSCIOUSNESS_GAMMA_FREQ_HZ;
    config.phase_coherence_threshold = SWARM_CONSCIOUSNESS_PHASE_COHERENCE_THRESHOLD;

    // Hierarchy settings
    config.enable_hierarchy = true;
    config.squad_size = 4;
    config.platoon_size = 12;

    // Resilience settings
    config.enable_resilience = true;
    config.simulated_dropout_rate = 0.1f;

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create enhanced consciousness context
 * WHY:  Initialize all enhanced features
 * HOW:  Allocate, validate, initialize subsystems
 */
swarm_consciousness_enhanced_ctx_t* swarm_consciousness_enhanced_create(
    const swarm_consciousness_enhanced_config_t* config)
{
    // Use defaults if no config provided
    swarm_consciousness_enhanced_config_t effective_config;
    if (config) {
        memcpy(&effective_config, config, sizeof(effective_config));
    } else {
        effective_config = swarm_consciousness_enhanced_default_config();
    }

    // Allocate context
    swarm_consciousness_enhanced_ctx_t* ctx = (swarm_consciousness_enhanced_ctx_t*)
        nimcp_calloc(1, sizeof(swarm_consciousness_enhanced_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_consciousness_enhanced_create: failed to allocate context");
        return NULL;
    }

    // Initialize magic and config
    ctx->magic = ENHANCED_CONSCIOUSNESS_MAGIC;
    memcpy(&ctx->config, &effective_config, sizeof(effective_config));

    // Initialize mutex
    if (nimcp_mutex_init(&ctx->lock, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_consciousness_enhanced_create: failed to initialize mutex");
        nimcp_free(ctx);
        return NULL;
    }

    // Initialize state
    ctx->attached = false;
    ctx->attached_swarm = NULL;
    ctx->current_phase = CONSCIOUSNESS_PHASE_CHAOS;
    ctx->binding_active = false;
    ctx->bio_async_registered = false;
    ctx->creation_time_ms = enhanced_get_time_ms();

    // Initialize collections
    ctx->remote_phi_count = 0;
    ctx->pending_request_count = 0;
    ctx->history_count = 0;
    ctx->history_index = 0;
    ctx->local_phi = 0.0f;

    // Initialize callbacks
    ctx->peer_callback = NULL;
    ctx->phase_callback = NULL;
    ctx->binding_callback = NULL;

    // Register with BBB
    if (!bbb_register_module(MODULE_NAME, BBB_MODULE_TYPE_SWARM)) {
        LOG_WARN("Failed to register with BBB security");
    }

    LOG_INFO("Enhanced consciousness context created");
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "create",
                  "geometry=%d dynamics=%d binding=%d",
                  effective_config.enable_geometry,
                  effective_config.enable_dynamics,
                  effective_config.enable_binding);

    return ctx;
}

/**
 * WHAT: Destroy enhanced consciousness context
 * WHY:  Clean up all resources
 * HOW:  Detach, unregister, free
 */
void swarm_consciousness_enhanced_destroy(swarm_consciousness_enhanced_ctx_t* ctx) {
    if (!ctx) return;

    // Validate magic
    if (ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context magic in destroy");
        return;
    }

    // Detach from swarm if attached
    if (ctx->attached) {
        swarm_consciousness_detach_from_swarm(ctx);
    }

    // Unregister from BBB
    bbb_unregister_module(MODULE_NAME);

    // Destroy mutex
    nimcp_mutex_destroy(&ctx->lock);

    // Clear magic
    ctx->magic = 0;

    // Free context
    nimcp_free(ctx);

    LOG_INFO("Enhanced consciousness context destroyed");
}

//=============================================================================
// Peer Event Functions
//=============================================================================

/**
 * WHAT: Register peer event callback
 * WHY:  Notify application of peer changes
 * HOW:  Store callback pointer
 */
bool swarm_consciousness_register_peer_callback(
    swarm_consciousness_enhanced_ctx_t* ctx,
    peer_event_callback_t callback,
    void* user_data)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_register_peer_callback: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->peer_callback = callback;
    ctx->peer_callback_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("Peer event callback registered");
    return true;
}

/**
 * WHAT: Unregister peer event callback
 * WHY:  Stop notifications
 * HOW:  Clear callback pointer
 */
void swarm_consciousness_unregister_peer_callback(
    swarm_consciousness_enhanced_ctx_t* ctx)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) return;

    nimcp_mutex_lock(&ctx->lock);
    ctx->peer_callback = NULL;
    ctx->peer_callback_data = NULL;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("Peer event callback unregistered");
}

/**
 * WHAT: Handle peer joined event
 * WHY:  Update consciousness on swarm membership change
 * HOW:  Request phi, invoke callback, recompute consciousness
 */
bool swarm_consciousness_on_peer_joined(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_on_peer_joined: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->peer_join_count++;

    // Auto-request phi if enabled
    if (ctx->config.auto_collect_phi_on_join && ctx->attached) {
        nimcp_mutex_unlock(&ctx->lock);
        swarm_consciousness_request_phi(ctx, drone_id);
        nimcp_mutex_lock(&ctx->lock);
    }

    // Get current drone count
    uint32_t drone_count = ctx->remote_phi_count + 1;  // +1 for self

    nimcp_mutex_unlock(&ctx->lock);

    // Invoke callback
    enhanced_invoke_peer_callback(ctx, PEER_EVENT_JOINED, drone_id, 0.0f, drone_count);

    LOG_INFO("Peer joined gestalt: drone_id=%u, total=%u", drone_id, drone_count);
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "peer_joined",
                  "drone_id=%u count=%u", drone_id, drone_count);

    return true;
}

/**
 * WHAT: Handle peer left event
 * WHY:  Update consciousness on swarm membership change
 * HOW:  Remove phi, invoke callback, recompute consciousness
 */
bool swarm_consciousness_on_peer_left(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id,
    bool graceful)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_on_peer_left: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->peer_leave_count++;

    // Remove drone's phi from collection
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        if (ctx->remote_phi[i].drone_id == drone_id) {
            // Shift remaining entries
            for (uint32_t j = i; j < ctx->remote_phi_count - 1; j++) {
                ctx->remote_phi[j] = ctx->remote_phi[j + 1];
            }
            ctx->remote_phi_count--;
            break;
        }
    }

    uint32_t drone_count = ctx->remote_phi_count + 1;

    nimcp_mutex_unlock(&ctx->lock);

    // Invoke callback
    peer_event_type_t event_type = graceful ? PEER_EVENT_LEFT : PEER_EVENT_TIMEOUT;
    enhanced_invoke_peer_callback(ctx, event_type, drone_id, 0.0f, drone_count);

    LOG_INFO("Peer left gestalt: drone_id=%u, graceful=%d, total=%u",
             drone_id, graceful, drone_count);
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "peer_left",
                  "drone_id=%u graceful=%d count=%u", drone_id, graceful, drone_count);

    return true;
}

//=============================================================================
// Phase Transition Functions
//=============================================================================

/**
 * WHAT: Register phase transition callback
 * WHY:  Notify on consciousness phase changes
 * HOW:  Store callback pointer
 */
bool swarm_consciousness_register_phase_callback(
    swarm_consciousness_enhanced_ctx_t* ctx,
    phase_transition_callback_t callback,
    void* user_data)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_register_phase_callback: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->phase_callback = callback;
    ctx->phase_callback_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * WHAT: Detect phase transition from phi history
 * WHY:  Identify qualitative state changes
 * HOW:  Compare current phase to computed phase
 */
consciousness_phase_t swarm_consciousness_detect_phase_transition(
    swarm_consciousness_enhanced_ctx_t* ctx,
    bool* detected_transition)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        if (detected_transition) *detected_transition = false;
        return CONSCIOUSNESS_PHASE_CHAOS;
    }

    consciousness_dynamics_t dynamics;
    swarm_compute_consciousness_dynamics(ctx, &dynamics);

    nimcp_mutex_lock(&ctx->lock);
    consciousness_phase_t old_phase = ctx->current_phase;
    nimcp_mutex_unlock(&ctx->lock);

    if (detected_transition) {
        *detected_transition = (dynamics.current_phase != old_phase);
    }

    return dynamics.current_phase;
}

/**
 * WHAT: Get current consciousness phase
 * WHY:  Access current dynamical state
 * HOW:  Return stored phase
 */
consciousness_phase_t swarm_consciousness_get_phase(
    const swarm_consciousness_enhanced_ctx_t* ctx)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        return CONSCIOUSNESS_PHASE_CHAOS;
    }

    return ctx->current_phase;
}

//=============================================================================
// Neural Binding Functions
//=============================================================================

/**
 * WHAT: Register neural binding callback
 * WHY:  Notify on binding events
 * HOW:  Store callback pointer
 */
bool swarm_consciousness_register_binding_callback(
    swarm_consciousness_enhanced_ctx_t* ctx,
    binding_event_callback_t callback,
    void* user_data)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_register_binding_callback: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->binding_callback = callback;
    ctx->binding_callback_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * WHAT: Check if swarm has achieved neural binding
 * WHY:  Quick binding status check
 * HOW:  Compare phase coherence to threshold
 */
bool swarm_consciousness_is_bound(
    const swarm_consciousness_enhanced_ctx_t* ctx,
    float coherence_threshold)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        return false;
    }

    float threshold = (coherence_threshold > 0) ?
                      coherence_threshold :
                      ctx->config.phase_coherence_threshold;

    return ctx->binding_active && ctx->current_binding.phase_coherence >= threshold;
}

/**
 * WHAT: Get current binding metrics
 * WHY:  Access binding state
 * HOW:  Copy current binding
 */
bool swarm_consciousness_get_binding(
    const swarm_consciousness_enhanced_ctx_t* ctx,
    neural_binding_t* binding)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC || !binding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_get_binding: required parameter is NULL (ctx, binding)");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&ctx->lock);
    memcpy(binding, &ctx->current_binding, sizeof(neural_binding_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)&ctx->lock);

    return true;
}

//=============================================================================
// Swarm Brain Integration Functions
//=============================================================================

/**
 * WHAT: Attach enhanced consciousness to swarm brain
 * WHY:  Enable automatic updates on peer changes
 * HOW:  Register internal handlers with swarm_brain
 */
bool swarm_consciousness_attach_to_swarm(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC || !swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_attach_to_swarm: required parameter is NULL (ctx, swarm)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (ctx->attached) {
        LOG_WARN("Already attached to a swarm");
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_attach_to_swarm: validation failed");
        return false;
    }

    ctx->attached_swarm = swarm;
    ctx->attached = true;

    // Get local phi
    brain_t local_brain = swarm_brain_get_local_brain(swarm);
    if (local_brain) {
        introspection_context_t introspection = brain_get_introspection(local_brain);
        if (introspection) {
            consciousness_phi_result_t* phi_result =
                introspection_compute_phi_fast(introspection, NULL);
            if (phi_result) {
                ctx->local_phi = phi_result->phi;
                nimcp_free(phi_result);
            }
        }
    }

    nimcp_mutex_unlock(&ctx->lock);

    // Request phi from all existing peers
    if (ctx->config.auto_collect_phi_on_join) {
        swarm_consciousness_request_all_phi(ctx);
    }

    LOG_INFO("Enhanced consciousness attached to swarm");
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "attach",
                  "local_phi=%.3f", ctx->local_phi);

    return true;
}

/**
 * WHAT: Detach enhanced consciousness from swarm brain
 * WHY:  Cleanup on shutdown
 * HOW:  Unregister handlers, clear reference
 */
void swarm_consciousness_detach_from_swarm(
    swarm_consciousness_enhanced_ctx_t* ctx)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        return;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (!ctx->attached) {
        nimcp_mutex_unlock(&ctx->lock);
        return;
    }

    ctx->attached_swarm = NULL;
    ctx->attached = false;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("Enhanced consciousness detached from swarm");
}

/**
 * WHAT: Set signal adapter for phi messaging
 * WHY:  Decouples consciousness from signal layer for flexible integration
 * HOW:  Store adapter reference for use in phi request/response functions
 */
bool swarm_consciousness_set_signal_adapter(
    swarm_consciousness_enhanced_ctx_t* ctx,
    nimcp_swarm_signal_adapter_t* adapter)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_set_signal_adapter: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->signal_adapter = adapter;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("Signal adapter %s for enhanced consciousness",
              adapter ? "set" : "cleared");
    return true;
}

/**
 * WHAT: Process swarm protocol message for consciousness
 * WHY:  Handle PHI_REQUEST and PHI_RESPONSE
 * HOW:  Parse message, dispatch to handler
 */
bool swarm_consciousness_handle_protocol_message(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint8_t msg_type,
    const uint8_t* data,
    uint32_t len,
    uint16_t source_drone_id)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_handle_protocol_message: ctx is NULL");
        return false;
    }

    switch (msg_type) {
        case SWARM_MSG_PHI_REQUEST: {
            // Respond with our phi
            if (!ctx->attached || !ctx->attached_swarm) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_handle_protocol_message: required parameter is NULL (ctx->attached, ctx->attached_swarm)");
                return false;
            }

            nimcp_mutex_lock(&ctx->lock);
            float phi = ctx->local_phi;
            nimcp_mutex_unlock(&ctx->lock);

            // Send response
            uint8_t response[5];
            response[0] = SWARM_MSG_PHI_RESPONSE;
            memcpy(&response[1], &phi, sizeof(float));

            if (ctx->signal_adapter) {
                swarm_signal_send(ctx->signal_adapter, response, sizeof(response), source_drone_id);
                LOG_DEBUG("Sent phi response to drone %u: %.3f", source_drone_id, phi);
            }
            return true;
        }

        case SWARM_MSG_PHI_RESPONSE: {
            if (len < 4) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_handle_protocol_message: validation failed");
                return false;
            }

            float phi_value;
            memcpy(&phi_value, data, sizeof(float));

            return swarm_consciousness_handle_phi_response(ctx, source_drone_id, phi_value);
        }

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_handle_protocol_message: validation failed");
            return false;
    }
}

//=============================================================================
// Bio-Async Integration Functions
//=============================================================================

/**
 * WHAT: Register enhanced consciousness with bio-async
 * WHY:  Enable streaming updates
 * HOW:  Set flag, register handlers
 */
bool swarm_consciousness_enhanced_register_bio_async(
    swarm_consciousness_enhanced_ctx_t* ctx)
{
    if (!ctx || ctx->magic != ENHANCED_CONSCIOUSNESS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_enhanced_register_bio_async: ctx is NULL");
        return false;
    }

    if (!nimcp_bio_async_is_initialized()) {
        LOG_WARN("Bio-async not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "swarm_consciousness_enhanced_register_bio_async: nimcp_bio_async_is_initialized is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->bio_async_registered = true;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("Enhanced consciousness registered with bio-async");
    return true;
}

/**
 * WHAT: Publish phase transition via bio-async
 * WHY:  Notify external systems of state changes
 * HOW:  Send BIO_MSG_SWARM_PHASE_TRANSITION
 */
void swarm_consciousness_publish_phase_transition(
    swarm_consciousness_enhanced_ctx_t* ctx,
    consciousness_phase_t old_phase,
    consciousness_phase_t new_phase)
{
    if (!ctx || !ctx->bio_async_registered) {
        return;
    }

    LOG_INFO("Phase transition: %s -> %s",
             consciousness_phase_name(old_phase),
             consciousness_phase_name(new_phase));

    // In full implementation, would publish via bio-async router
}

/**
 * WHAT: Publish binding event via bio-async
 * WHY:  Notify external systems of binding changes
 * HOW:  Send BIO_MSG_SWARM_BINDING_EVENT
 */
void swarm_consciousness_publish_binding_event(
    swarm_consciousness_enhanced_ctx_t* ctx,
    const neural_binding_t* binding)
{
    if (!ctx || !binding || !ctx->bio_async_registered) {
        return;
    }

    LOG_INFO("Binding event: active=%d, coherence=%.3f, strength=%.3f",
             binding->binding_active, binding->phase_coherence, binding->binding_strength);

    // In full implementation, would publish via bio-async router
}

//=============================================================================
// Security Functions (BBB)
//=============================================================================

/**
 * WHAT: Validate enhanced consciousness metrics
 * WHY:  Ensure data integrity for BBB systems
 * HOW:  Range checks, NaN detection
 */
bool swarm_consciousness_enhanced_bbb_validate(
    const swarm_consciousness_enhanced_metrics_t* metrics)
{
    if (!metrics) {
        LOG_ERROR("Null metrics in BBB validation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_enhanced_bbb_validate: metrics is NULL");
        return false;
    }

    // Validate base metrics
    if (!swarm_consciousness_bbb_validate(&metrics->base)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_enhanced_bbb_validate: swarm_consciousness_bbb_validate is NULL");
        return false;
    }

    // Validate geometry bounds
    if (metrics->geometry.total_correlation < 0 ||
        isnan(metrics->geometry.total_correlation) ||
        isinf(metrics->geometry.total_correlation)) {
        LOG_ERROR("Invalid geometry: total_correlation=%.3f",
                  metrics->geometry.total_correlation);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_enhanced_bbb_validate: operation failed");
        return false;
    }

    // Validate dynamics bounds
    if (metrics->dynamics.current_phase < CONSCIOUSNESS_PHASE_CHAOS ||
        metrics->dynamics.current_phase > CONSCIOUSNESS_PHASE_FROZEN) {
        LOG_ERROR("Invalid phase: %d", metrics->dynamics.current_phase);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_enhanced_bbb_validate: operation failed");
        return false;
    }

    // Validate binding bounds
    if (metrics->binding.phase_coherence < 0 || metrics->binding.phase_coherence > 1.0f) {
        LOG_ERROR("Invalid binding coherence: %.3f", metrics->binding.phase_coherence);
        return false;
    }

    // Validate hierarchy
    for (int i = 0; i < SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS; i++) {
        if (metrics->hierarchy.phi_by_level[i] < 0 ||
            isnan(metrics->hierarchy.phi_by_level[i])) {
            LOG_ERROR("Invalid hierarchy phi[%d]: %.3f",
                      i, metrics->hierarchy.phi_by_level[i]);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_enhanced_bbb_validate: operation failed");
            return false;
        }
    }

    // Validate resilience
    if (metrics->resilience.fragility_index < 0 ||
        metrics->resilience.fragility_index > 1.0f) {
        LOG_ERROR("Invalid fragility: %.3f", metrics->resilience.fragility_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_enhanced_bbb_validate: operation failed");
        return false;
    }

    return true;
}

/**
 * WHAT: Validate phi protocol message
 * WHY:  Prevent malicious phi injection
 * HOW:  Check format, bounds, source
 */
bool swarm_consciousness_validate_phi_message(
    const uint8_t* data,
    uint32_t len,
    uint16_t source_drone_id)
{
    // Check minimum length
    if (!data || len < 4) {
        LOG_WARN("Invalid phi message length: %u", len);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_validate_phi_message: data is NULL");
        return false;
    }

    // Extract phi value
    float phi_value;
    memcpy(&phi_value, data, sizeof(float));

    // Validate phi bounds
    if (phi_value < 0 || phi_value > SWARM_CONSCIOUSNESS_MAX_PHI ||
        isnan(phi_value) || isinf(phi_value)) {
        LOG_WARN("Invalid phi value from drone %u: %.3f", source_drone_id, phi_value);
        bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "invalid_phi_msg",
                      "drone=%u phi=%.3f", source_drone_id, phi_value);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_validate_phi_message: operation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Get phase name string
 * WHY:  Human-readable output
 * HOW:  Switch on enum
 */
const char* consciousness_phase_name(consciousness_phase_t phase) {
    switch (phase) {
        case CONSCIOUSNESS_PHASE_CHAOS:    return "CHAOS";
        case CONSCIOUSNESS_PHASE_CRITICAL: return "CRITICAL";
        case CONSCIOUSNESS_PHASE_ORDERED:  return "ORDERED";
        case CONSCIOUSNESS_PHASE_FROZEN:   return "FROZEN";
        default:                           return "UNKNOWN";
    }
}

/**
 * WHAT: Get hierarchy level name string
 * WHY:  Human-readable output
 * HOW:  Switch on enum
 */
const char* consciousness_hierarchy_name(consciousness_hierarchy_t level) {
    switch (level) {
        case HIERARCHY_INDIVIDUAL: return "INDIVIDUAL";
        case HIERARCHY_SQUAD:      return "SQUAD";
        case HIERARCHY_PLATOON:    return "PLATOON";
        case HIERARCHY_SWARM:      return "SWARM";
        default:                   return "UNKNOWN";
    }
}

/**
 * WHAT: Free enhanced consciousness metrics
 * WHY:  Memory management
 * HOW:  Simple free (no sub-allocations)
 */
void swarm_consciousness_enhanced_metrics_free(
    swarm_consciousness_enhanced_metrics_t* metrics)
{
    if (metrics) {
        nimcp_free(metrics);
    }
}

/**
 * WHAT: Print enhanced metrics for debugging
 * WHY:  Development and debugging aid
 * HOW:  Formatted output to log
 */
void swarm_consciousness_enhanced_print_metrics(
    const swarm_consciousness_enhanced_metrics_t* metrics,
    bool verbose)
{
    if (!metrics) return;

    LOG_INFO("=== Enhanced Consciousness Metrics ===");
    LOG_INFO("Base: phi=%.3f, state=%s, drones=%u",
             metrics->base.collective_phi,
             swarm_consciousness_state_name(metrics->base.consciousness_state),
             metrics->base.drone_count);
    LOG_INFO("Remote: collected=%u, pending=%u, complete=%d",
             metrics->remote_phi_collected,
             metrics->remote_phi_pending,
             metrics->collection_complete);

    if (verbose) {
        LOG_INFO("Geometry: total_corr=%.3f, integration=%.3f, complexity=%.3f",
                 metrics->geometry.total_correlation,
                 metrics->geometry.integration,
                 metrics->geometry.complexity);
        LOG_INFO("Dynamics: phase=%s, lyapunov=%.3f, autocorr=%.3f, near_trans=%d",
                 consciousness_phase_name(metrics->dynamics.current_phase),
                 metrics->dynamics.lyapunov_exponent,
                 metrics->dynamics.autocorrelation,
                 metrics->dynamics.near_transition);
        LOG_INFO("Binding: active=%d, coherence=%.3f, strength=%.3f, bound=%u",
                 metrics->binding.binding_active,
                 metrics->binding.phase_coherence,
                 metrics->binding.binding_strength,
                 metrics->binding.bound_drone_count);
        LOG_INFO("Hierarchy: dominant=%s, cross_level=%.3f",
                 consciousness_hierarchy_name(metrics->hierarchy.dominant_level),
                 metrics->hierarchy.cross_level_integration);
        LOG_INFO("Resilience: baseline=%.3f, fragility=%.3f, redundant=%d",
                 metrics->resilience.baseline_phi,
                 metrics->resilience.fragility_index,
                 metrics->resilience.redundancy_sufficient);
    }
}
