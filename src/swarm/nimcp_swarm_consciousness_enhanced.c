/**
 * @file nimcp_swarm_consciousness_enhanced.c
 * @brief Implementation of Enhanced Swarm Gestalt Consciousness
 *
 * WHAT: Advanced collective consciousness features for drone swarms
 * WHY:  Enable true gestalt consciousness with automatic peer enrollment
 * HOW:  Peer callbacks, remote phi collection, information geometry, dynamics
 *
 * BIOLOGICAL BASIS:
 * - Peer joining → Neural recruitment in cortical assemblies
 * - Remote phi → Inter-region communication (corpus callosum)
 * - Information geometry → Effective connectivity patterns
 * - Phase transitions → Sleep/wake transitions, anesthesia emergence
 * - Gamma binding → 40Hz binding hypothesis (Crick & Koch)
 * - Resilience → Brain fault tolerance (plasticity, redundancy)
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 * @version 2.0.0
 */

#include "swarm/nimcp_swarm_consciousness_enhanced.h"
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_swarm_signal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/statistics/nimcp_statistics.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_consciousness_enhanced)

//=============================================================================
// Module Constants
//=============================================================================

#define MODULE_NAME "swarm_consciousness_enhanced"

/** Magic value for context validation */
#define ENHANCED_CONSCIOUSNESS_MAGIC 0x45434F4E  // 'ECON'

/** Minimum samples for geometry computation */
#define MIN_GEOMETRY_SAMPLES 10

/** Minimum samples for dynamics computation */
#define MIN_DYNAMICS_SAMPLES 20

/** Default entropy bins for mutual information */
#define DEFAULT_ENTROPY_BINS 16

/** Critical slowing autocorrelation threshold */
#define CRITICAL_AUTOCORRELATION_THRESHOLD 0.8f

/** Phase coherence epsilon for binding detection */
#define PHASE_COHERENCE_EPSILON 0.01f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * Remote phi storage for collected values
 */
typedef struct {
    uint16_t drone_id;
    float phi_value;
    uint64_t timestamp_ms;
    bool valid;
} remote_phi_entry_t;

/**
 * Enhanced consciousness context internal structure
 */
struct swarm_consciousness_enhanced_ctx {
    uint32_t magic;                          /**< Validation magic */
    swarm_consciousness_enhanced_config_t config;  /**< Configuration */
    nimcp_mutex_t lock;                    /**< Thread safety */

    /* Swarm reference */
    swarm_brain_t* attached_swarm;           /**< Attached swarm brain */
    bool attached;                           /**< Is attached to swarm? */

    /* Remote phi collection */
    remote_phi_entry_t remote_phi[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t remote_phi_count;
    phi_request_t pending_requests[SWARM_CONSCIOUSNESS_MAX_PHI_REQUESTS];
    uint32_t pending_request_count;
    float local_phi;                         /**< Local drone's phi */

    /* Phi history for dynamics */
    float phi_history[SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE];
    float phi_timestamps[SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE];
    uint32_t history_count;
    uint32_t history_index;

    /* Current state */
    consciousness_phase_t current_phase;
    neural_binding_t current_binding;
    bool binding_active;

    /* Callbacks */
    peer_event_callback_t peer_callback;
    void* peer_callback_data;
    phase_transition_callback_t phase_callback;
    void* phase_callback_data;
    binding_event_callback_t binding_callback;
    void* binding_callback_data;

    /* Statistics */
    uint64_t total_phi_requests;
    uint64_t total_phi_responses;
    uint64_t peer_join_count;
    uint64_t peer_leave_count;
    uint64_t phase_transitions;
    uint64_t binding_events;

    /* Bio-async */
    bool bio_async_registered;

    /* Signal adapter for phi messaging */
    nimcp_swarm_signal_adapter_t* signal_adapter;

    /* Creation time */
    uint64_t creation_time_ms;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static uint64_t get_time_ms(void);
static void add_phi_to_history(swarm_consciousness_enhanced_ctx_t* ctx, float phi);
static float compute_autocorrelation(const float* data, uint32_t count, uint32_t lag);
static float compute_variance(const float* data, uint32_t count);
static float compute_mean(const float* data, uint32_t count);
static void invoke_peer_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                  peer_event_type_t type, uint16_t drone_id,
                                  float phi, uint32_t new_count);
static void invoke_phase_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                   consciousness_phase_t old_phase,
                                   consciousness_phase_t new_phase,
                                   const swarm_consciousness_enhanced_metrics_t* metrics);
static void invoke_binding_callback(swarm_consciousness_enhanced_ctx_t* ctx,
                                     const neural_binding_t* binding);
static float estimate_entropy(const float* data, uint32_t count, uint32_t bins, float bin_width);
static float estimate_joint_entropy(const float* data1, const float* data2,
                                    uint32_t count, uint32_t bins, float bin_width);

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
    ctx->creation_time_ms = get_time_ms();

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
    invoke_peer_callback(ctx, PEER_EVENT_JOINED, drone_id, 0.0f, drone_count);

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
    invoke_peer_callback(ctx, event_type, drone_id, 0.0f, drone_count);

    LOG_INFO("Peer left gestalt: drone_id=%u, graceful=%d, total=%u",
             drone_id, graceful, drone_count);
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "peer_left",
                  "drone_id=%u graceful=%d count=%u", drone_id, graceful, drone_count);

    return true;
}

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
        req->request_time_ms = get_time_ms();
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
            ctx->remote_phi[i].timestamp_ms = get_time_ms();
            ctx->remote_phi[i].valid = true;
            found = true;
            break;
        }
    }

    if (!found && ctx->remote_phi_count < SWARM_CONSCIOUSNESS_MAX_DRONES) {
        remote_phi_entry_t* entry = &ctx->remote_phi[ctx->remote_phi_count++];
        entry->drone_id = drone_id;
        entry->phi_value = phi_value;
        entry->timestamp_ms = get_time_ms();
        entry->valid = true;
    }

    nimcp_mutex_unlock(&ctx->lock);

    // Invoke callback
    invoke_peer_callback(ctx, PEER_EVENT_PHI_UPDATE, drone_id, phi_value,
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

    // Allocate result
    swarm_consciousness_enhanced_metrics_t* metrics =
        (swarm_consciousness_enhanced_metrics_t*)nimcp_calloc(1, sizeof(*metrics));
    if (!metrics) {
        LOG_ERROR("Failed to allocate enhanced metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_compute_enhanced_metrics: metrics is NULL");
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
    add_phi_to_history(ctx, metrics->base.collective_phi);

    // Copy history
    metrics->history_count = ctx->history_count;
    metrics->history_index = ctx->history_index;
    memcpy(metrics->phi_history, ctx->phi_history, sizeof(ctx->phi_history));

    nimcp_mutex_unlock(&ctx->lock);

    // Compute advanced metrics if enabled
    if (ctx->config.enable_geometry && ctx->history_count >= MIN_GEOMETRY_SAMPLES) {
        swarm_compute_information_geometry(ctx, &metrics->geometry);
    }

    if (ctx->config.enable_dynamics && ctx->history_count >= MIN_DYNAMICS_SAMPLES) {
        swarm_compute_consciousness_dynamics(ctx, &metrics->dynamics);

        // Check for phase transition
        bool transition_detected = false;
        consciousness_phase_t new_phase = swarm_consciousness_detect_phase_transition(
            ctx, &transition_detected);
        if (transition_detected) {
            invoke_phase_callback(ctx, ctx->current_phase, new_phase, metrics);
            nimcp_mutex_lock(&ctx->lock);
            ctx->current_phase = new_phase;
            ctx->phase_transitions++;
            nimcp_mutex_unlock(&ctx->lock);
        }
    }

    if (ctx->config.enable_binding) {
        swarm_compute_neural_binding(ctx, &metrics->binding);

        // Check for binding event
        if (metrics->binding.binding_active && !ctx->binding_active) {
            invoke_binding_callback(ctx, &metrics->binding);
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
        swarm_compute_hierarchical_consciousness(ctx, swarm, &metrics->hierarchy);
    }

    if (ctx->config.enable_resilience) {
        swarm_compute_consciousness_resilience(ctx, swarm, &metrics->resilience);
    }

    LOG_DEBUG("Enhanced metrics computed: phi=%.3f, geometry=%d, dynamics=%d, binding=%d",
              metrics->base.collective_phi,
              ctx->config.enable_geometry,
              ctx->config.enable_dynamics,
              ctx->config.enable_binding);

    return metrics;
}

/**
 * WHAT: Compute information geometry metrics
 * WHY:  Deep analysis of information integration
 * HOW:  Compute mutual info, transfer entropy from phi history
 */
bool swarm_compute_information_geometry(
    swarm_consciousness_enhanced_ctx_t* ctx,
    information_geometry_t* geometry)
{
    if (!ctx || !geometry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_information_geometry: required parameter is NULL (ctx, geometry)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (ctx->history_count < MIN_GEOMETRY_SAMPLES) {
        nimcp_mutex_unlock(&ctx->lock);
        memset(geometry, 0, sizeof(*geometry));
        return false;  /* Insufficient history is normal during warmup */
    }

    uint32_t n = ctx->history_count;
    uint32_t bins = DEFAULT_ENTROPY_BINS;
    float bin_width = ctx->config.entropy_bin_width;

    // Compute entropy of phi history
    float H_phi = estimate_entropy(ctx->phi_history, n, bins, bin_width);

    // For full mutual information matrix, we need per-drone histories
    // For now, compute simplified total correlation from phi series
    geometry->total_correlation = H_phi;

    // Integration approximation: difference between joint and sum of marginals
    geometry->integration = H_phi * 0.5f;  // Simplified

    // Complexity: integration weighted by information content
    geometry->complexity = geometry->integration * H_phi;

    // Redundancy: shared information (simplified estimate)
    geometry->redundancy = geometry->total_correlation - geometry->integration;
    if (geometry->redundancy < 0) geometry->redundancy = 0;

    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * WHAT: Compute consciousness dynamics metrics
 * WHY:  Detect phase transitions, criticality
 * HOW:  Time series analysis of phi trajectory
 */
bool swarm_compute_consciousness_dynamics(
    swarm_consciousness_enhanced_ctx_t* ctx,
    consciousness_dynamics_t* dynamics)
{
    if (!ctx || !dynamics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_consciousness_dynamics: required parameter is NULL (ctx, dynamics)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (ctx->history_count < MIN_DYNAMICS_SAMPLES) {
        nimcp_mutex_unlock(&ctx->lock);
        memset(dynamics, 0, sizeof(*dynamics));
        dynamics->current_phase = CONSCIOUSNESS_PHASE_CHAOS;
        return false;  /* Insufficient history is normal during warmup */
    }

    uint32_t n = ctx->history_count;

    // Compute variance
    float variance = compute_variance(ctx->phi_history, n);

    // Compute autocorrelation at lag 1
    float autocorr = compute_autocorrelation(ctx->phi_history, n, 1);

    // Compute variance trend (change over window)
    float early_var = compute_variance(ctx->phi_history, n / 2);
    float late_var = compute_variance(ctx->phi_history + n / 2, n / 2);
    float var_trend = (late_var - early_var) / (early_var + 0.001f);

    // Approximate Lyapunov exponent from divergence
    float lyap = 0.0f;
    if (n > 2) {
        float sum_log_div = 0.0f;
        uint32_t count = 0;
        for (uint32_t i = 1; i < n; i++) {
            float diff = fabsf(ctx->phi_history[i] - ctx->phi_history[i-1]);
            if (diff > 0.001f) {
                sum_log_div += logf(diff);
                count++;
            }
        }
        if (count > 0) {
            lyap = sum_log_div / count;
        }
    }

    // Determine phase based on metrics
    consciousness_phase_t phase;
    if (lyap > 0.5f && variance > ctx->config.critical_variance_threshold) {
        phase = CONSCIOUSNESS_PHASE_CHAOS;
    } else if (autocorr > CRITICAL_AUTOCORRELATION_THRESHOLD) {
        phase = CONSCIOUSNESS_PHASE_CRITICAL;
    } else if (variance < ctx->config.critical_variance_threshold * 0.1f) {
        phase = CONSCIOUSNESS_PHASE_FROZEN;
    } else {
        phase = CONSCIOUSNESS_PHASE_ORDERED;
    }

    // Detect near-transition
    bool near_transition = (autocorr > CRITICAL_AUTOCORRELATION_THRESHOLD * 0.8f) ||
                          (fabsf(var_trend) > 0.5f);

    // Fill dynamics struct
    dynamics->current_phase = phase;
    dynamics->lyapunov_exponent = lyap;
    dynamics->autocorrelation = autocorr;
    dynamics->variance_trend = var_trend;
    dynamics->attractor_strength = 1.0f - fabsf(lyap);
    dynamics->near_transition = near_transition;
    dynamics->transition_probability = near_transition ? 0.3f : 0.05f;

    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * WHAT: Compute neural binding metrics
 * WHY:  Detect gamma-band synchronization
 * HOW:  Phase coherence analysis of phi oscillations
 */
bool swarm_compute_neural_binding(
    swarm_consciousness_enhanced_ctx_t* ctx,
    neural_binding_t* binding)
{
    if (!ctx || !binding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_neural_binding: required parameter is NULL (ctx, binding)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    memset(binding, 0, sizeof(*binding));

    if (ctx->history_count < 10) {
        nimcp_mutex_unlock(&ctx->lock);
        return false;  /* Insufficient history is normal during warmup */
    }

    // Estimate gamma power from high-frequency phi fluctuations
    float gamma_power = 0.0f;
    float phase_sum_cos = 0.0f;
    float phase_sum_sin = 0.0f;
    uint32_t n = ctx->history_count;

    // Simple gamma estimation: high-frequency variance
    for (uint32_t i = 1; i < n; i++) {
        float diff = ctx->phi_history[i] - ctx->phi_history[i-1];
        gamma_power += diff * diff;

        // Estimate phase from rate of change
        float phase = atan2f(diff, ctx->phi_history[i]);
        phase_sum_cos += cosf(phase);
        phase_sum_sin += sinf(phase);
    }
    gamma_power /= (n - 1);

    // Phase coherence (PLV) from mean resultant length
    float mean_cos = phase_sum_cos / (n - 1);
    float mean_sin = phase_sum_sin / (n - 1);
    float phase_coherence = sqrtf(mean_cos * mean_cos + mean_sin * mean_sin);
    float mean_phase = atan2f(mean_sin, mean_cos);

    // Binding strength combines power and coherence
    float binding_strength = gamma_power * phase_coherence;
    bool binding_active = phase_coherence > ctx->config.phase_coherence_threshold;

    // Count bound drones (those contributing to coherence)
    uint32_t bound_count = binding_active ? (ctx->remote_phi_count + 1) : 0;

    // Fill binding struct
    binding->gamma_power = gamma_power;
    binding->phase_coherence = phase_coherence;
    binding->mean_phase = mean_phase;
    binding->binding_strength = binding_strength;
    binding->binding_active = binding_active;
    binding->bound_drone_count = bound_count;

    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * WHAT: Compute hierarchical consciousness metrics
 * WHY:  Analyze consciousness at different scales
 * HOW:  Recursive aggregation from individuals to swarm
 */
bool swarm_compute_hierarchical_consciousness(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    hierarchical_consciousness_t* hierarchy)
{
    if (!ctx || !swarm || !hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_hierarchical_consciousness: required parameter is NULL (ctx, swarm, hierarchy)");
        return false;
    }

    memset(hierarchy, 0, sizeof(*hierarchy));

    // Get peer info
    uint32_t peer_count = 0;
    const swarm_peer_info_t* peers = swarm_brain_get_peers(swarm, &peer_count);
    uint32_t total_drones = peer_count + 1;

    nimcp_mutex_lock(&ctx->lock);

    // Level 0: Individual (local phi only)
    hierarchy->phi_by_level[HIERARCHY_INDIVIDUAL] = ctx->local_phi;
    hierarchy->state_by_level[HIERARCHY_INDIVIDUAL] =
        swarm_classify_collective_phi(ctx->local_phi, 1);

    // Level 1: Squad (small group)
    uint32_t squad_size = (total_drones < ctx->config.squad_size) ?
                          total_drones : ctx->config.squad_size;
    float squad_phi = ctx->local_phi;
    for (uint32_t i = 0; i < squad_size - 1 && i < ctx->remote_phi_count; i++) {
        squad_phi += ctx->remote_phi[i].phi_value;
    }
    hierarchy->phi_by_level[HIERARCHY_SQUAD] = squad_phi;
    hierarchy->state_by_level[HIERARCHY_SQUAD] =
        swarm_classify_collective_phi(squad_phi, squad_size);

    // Level 2: Platoon
    uint32_t platoon_size = (total_drones < ctx->config.platoon_size) ?
                            total_drones : ctx->config.platoon_size;
    float platoon_phi = ctx->local_phi;
    for (uint32_t i = 0; i < platoon_size - 1 && i < ctx->remote_phi_count; i++) {
        platoon_phi += ctx->remote_phi[i].phi_value;
    }
    hierarchy->phi_by_level[HIERARCHY_PLATOON] = platoon_phi;
    hierarchy->state_by_level[HIERARCHY_PLATOON] =
        swarm_classify_collective_phi(platoon_phi, platoon_size);

    // Level 3: Full swarm
    float swarm_phi = ctx->local_phi;
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        swarm_phi += ctx->remote_phi[i].phi_value;
    }
    hierarchy->phi_by_level[HIERARCHY_SWARM] = swarm_phi;
    hierarchy->state_by_level[HIERARCHY_SWARM] =
        swarm_classify_collective_phi(swarm_phi, total_drones);

    nimcp_mutex_unlock(&ctx->lock);

    // Cross-level integration: how much does adding levels increase phi?
    float level_sum = 0.0f;
    for (int i = 0; i < SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS; i++) {
        level_sum += hierarchy->phi_by_level[i];
    }
    hierarchy->cross_level_integration = (swarm_phi > 0) ?
        (swarm_phi / (level_sum / SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS)) : 0.0f;

    // Dominant level: where is phi density highest?
    float max_density = 0.0f;
    hierarchy->dominant_level = HIERARCHY_INDIVIDUAL;
    float level_sizes[] = {1.0f, fmaxf(1.0f, (float)squad_size), fmaxf(1.0f, (float)platoon_size), fmaxf(1.0f, (float)total_drones)};
    for (int i = 0; i < SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS; i++) {
        float density = hierarchy->phi_by_level[i] / level_sizes[i];
        if (density > max_density) {
            max_density = density;
            hierarchy->dominant_level = (consciousness_hierarchy_t)i;
        }
    }

    return true;
}

/**
 * WHAT: Compute consciousness resilience metrics
 * WHY:  Measure robustness to drone loss
 * HOW:  Simulated dropout analysis
 */
bool swarm_compute_consciousness_resilience(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    consciousness_resilience_t* resilience)
{
    if (!ctx || !swarm || !resilience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_consciousness_resilience: required parameter is NULL (ctx, swarm, resilience)");
        return false;
    }

    memset(resilience, 0, sizeof(*resilience));

    nimcp_mutex_lock(&ctx->lock);

    uint32_t total_drones = ctx->remote_phi_count + 1;

    // Baseline phi with all drones
    float baseline_phi = ctx->local_phi;
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        baseline_phi += ctx->remote_phi[i].phi_value;
    }
    resilience->baseline_phi = baseline_phi;

    // Simulate single dropout: compute phi without each drone
    float sum_dropout_phi = 0.0f;
    float min_dropout_phi = baseline_phi;
    for (uint32_t d = 0; d < ctx->remote_phi_count; d++) {
        float dropout_phi = ctx->local_phi;
        for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
            if (i != d) {
                dropout_phi += ctx->remote_phi[i].phi_value;
            }
        }
        sum_dropout_phi += dropout_phi;
        if (dropout_phi < min_dropout_phi) {
            min_dropout_phi = dropout_phi;
        }
    }

    // Dropout sensitivity: average phi loss per dropout
    if (ctx->remote_phi_count > 0 && baseline_phi > 0.0f) {
        float avg_dropout_phi = sum_dropout_phi / ctx->remote_phi_count;
        resilience->dropout_sensitivity = (baseline_phi - avg_dropout_phi) / baseline_phi;
    }

    // Critical drone count: minimum for EMERGING state
    resilience->critical_drone_count = SWARM_CONSCIOUSNESS_DEFAULT_MIN_DRONES;

    // Fragility: how much does worst-case dropout hurt?
    if (baseline_phi > 0) {
        resilience->fragility_index = (baseline_phi - min_dropout_phi) / baseline_phi;
    }

    // Recovery rate: estimated (depends on re-joining dynamics)
    resilience->recovery_rate = 0.5f;  // 50% per update cycle (placeholder)

    // Redundancy check: can we survive any single failure?
    float min_remaining_phi = baseline_phi - resilience->fragility_index * baseline_phi;
    resilience->redundancy_sufficient =
        (min_remaining_phi >= SWARM_CONSCIOUSNESS_MIN_PHI_THRESHOLD * (total_drones - 1));

    nimcp_mutex_unlock(&ctx->lock);

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

//=============================================================================
// Internal Helper Implementations
//=============================================================================

static uint64_t get_time_ms(void) {
    return nimcp_time_get_ms();
}

static void add_phi_to_history(swarm_consciousness_enhanced_ctx_t* ctx, float phi) {
    ctx->phi_history[ctx->history_index] = phi;
    ctx->phi_timestamps[ctx->history_index] = (float)get_time_ms();
    ctx->history_index = (ctx->history_index + 1) % SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE;
    if (ctx->history_count < SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE) {
        ctx->history_count++;
    }
}

static float compute_mean(const float* data, uint32_t count) {
    if (!data || count == 0) return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum / count;
}

static float compute_variance(const float* data, uint32_t count) {
    if (!data || count < 2) return 0.0f;
    float mean = compute_mean(data, count);
    float variance = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    return variance / count;
}

static float compute_autocorrelation(const float* data, uint32_t count, uint32_t lag) {
    if (!data || count <= lag + 1) return 0.0f;

    float mean = compute_mean(data, count);
    float variance = compute_variance(data, count);
    if (variance < 1e-10f) return 0.0f;

    float autocorr = 0.0f;
    uint32_t n = count - lag;
    for (uint32_t i = 0; i < n; i++) {
        autocorr += (data[i] - mean) * (data[i + lag] - mean);
    }
    autocorr /= (n * variance);

    return autocorr;
}

static float estimate_entropy(const float* data, uint32_t count, uint32_t bins, float bin_width) {
    (void)bin_width;  /* bin_width handled internally by statistics module */
    if (!data || count == 0 || bins == 0) return 0.0f;

    /* Use central statistics module for differential entropy estimation */
    return nimcp_stats_differential_entropy(data, count, bins);
}

static float estimate_joint_entropy(const float* data1, const float* data2,
                                    uint32_t count, uint32_t bins, float bin_width) {
    (void)data1;
    (void)data2;
    (void)count;
    (void)bins;
    (void)bin_width;
    // Simplified: for full implementation, create 2D histogram
    return 0.0f;
}

static void invoke_peer_callback(swarm_consciousness_enhanced_ctx_t* ctx,
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
            .timestamp_ms = get_time_ms(),
            .new_drone_count = new_count
        };
        callback(&event, user_data);
    }
}

static void invoke_phase_callback(swarm_consciousness_enhanced_ctx_t* ctx,
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

static void invoke_binding_callback(swarm_consciousness_enhanced_ctx_t* ctx,
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
