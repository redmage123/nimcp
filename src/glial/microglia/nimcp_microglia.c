/**
 * @file nimcp_microglia.c
 * @brief Enhanced Microglia Implementation with Bio-Async Integration
 *
 * FEATURES:
 * - Bio-async messaging for immune/maintenance alerts via NOREPINEPHRINE channel
 * - KD-tree spatial indexing for O(log n) queries
 * - RK4 ODE integration for state dynamics
 * - Centrality-protected pruning with async event publishing
 * - Complement cascade (C1q/C3) tagging
 * - Cytokine signaling system
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.1.0 - Bio-Async Integrated
 */

#include "glial/microglia/nimcp_microglia.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/time/nimcp_time.h"
#include "utils/numerical/nimcp_integration.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(microglia)

//=============================================================================
// Global Bio-Async Context
//=============================================================================

static bio_module_context_t g_microglia_bio_ctx = NULL;
static unified_mem_manager_t g_microglia_mem_mgr = NULL;
static nimcp_atomic_bool_t g_microglia_bio_initialized = { .value = false };
static nimcp_platform_once_t g_microglia_bio_once = NIMCP_PLATFORM_ONCE_INIT;
static nimcp_error_t g_microglia_bio_init_result = NIMCP_SUCCESS;

//=============================================================================
// INTERNAL CONSTANTS
//=============================================================================

/** @brief C1q to C3 conversion time (seconds) */
#define C1Q_TO_C3_CONVERSION_TIME_S 2.0f

/** @brief Activity filter alpha (derived from cutoff) */
#define DEFAULT_FILTER_ALPHA 0.1f

/** @brief State ODE indices */
#define STATE_IDX_INFLAMMATION 0
#define STATE_IDX_ACTIVATION 1
#define STATE_IDX_PROCESS 2
#define STATE_IDX_ENERGY 3

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle BIO_MSG_MICROGLIA_ALERT message
 *
 * Processes alert via NOREPINEPHRINE channel (alerting/priority escalation).
 * High severity alerts trigger state transitions.
 */
static nimcp_error_t handle_microglia_alert_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("MICROGLIA", "Invalid alert message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // Parse alert payload (region, type, severity)
    if (msg_size < sizeof(bio_message_header_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(float)) {
        LOG_MODULE_ERROR("MICROGLIA", "Alert message too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    uint32_t alert_region = *(const uint32_t*)payload;
    uint32_t alert_type = *(const uint32_t*)(payload + sizeof(uint32_t));
    float severity = *(const float*)(payload + 2 * sizeof(uint32_t));

    LOG_MODULE_WARN("MICROGLIA", "Alert received: region=%u, type=%u, severity=%.2f",
                    alert_region, alert_type, severity);

    // Publish via NOREPINEPHRINE (alerting) channel
    nimcp_error_t result = bio_router_publish_signal(g_microglia_bio_ctx,
        "microglia.alert_severity", severity);

    // If high severity, escalate state
    if (severity > 0.7F) {
        LOG_MODULE_INFO("MICROGLIA", "High severity alert - escalating state");
        bio_router_publish_signal(g_microglia_bio_ctx, "microglia.state_escalation", 1.0F);
    }

    return result;
}

/**
 * @brief Handle BIO_MSG_MICROGLIA_PRUNE_REQUEST message
 *
 * Processes pruning requests and publishes pruning decisions.
 */
static nimcp_error_t handle_microglia_prune_request_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("MICROGLIA", "Invalid prune request message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Parse synapse ID from payload (G7: uint64 synapse_id).
    if (msg_size < sizeof(bio_message_header_t) + sizeof(uint64_t)) {
        LOG_MODULE_ERROR("MICROGLIA", "Prune request message too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    uint64_t synapse_id = *(const uint64_t*)payload;

    LOG_MODULE_DEBUG("MICROGLIA", "Prune request for synapse %llu",
                     (unsigned long long)synapse_id);

    // Publish pruning decision via NOREPINEPHRINE
    bio_router_publish_signal(g_microglia_bio_ctx, "microglia.pruning", 1.0F);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle BIO_MSG_METABOLIC_DEMAND message
 *
 * Microglia respond to metabolic distress by modulating their state.
 * High metabolic demand may trigger inflammatory responses or state transitions.
 */
static nimcp_error_t handle_metabolic_demand_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_msg_metabolic_demand_t)) {
        LOG_MODULE_ERROR("MICROGLIA", "Invalid metabolic demand message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_msg_metabolic_demand_t* demand = (const bio_msg_metabolic_demand_t*)msg;

    LOG_MODULE_DEBUG("MICROGLIA", "Metabolic demand from region %u: ATP deficit=%.2f, urgency=%.2f",
                     demand->region_id, demand->atp_deficit, demand->urgency);

    // High ATP deficit may indicate neuronal distress - trigger alert
    if (demand->atp_deficit > 0.5F || demand->urgency > 0.7F) {
        LOG_MODULE_WARN("MICROGLIA", "High metabolic distress detected - activating surveillance");
        bio_router_publish_signal(g_microglia_bio_ctx, "microglia.metabolic_distress", demand->atp_deficit);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle BIO_MSG_GLIAL_SYNC_REQUEST message
 *
 * Handles synchronization requests for coordinated glial cell activity.
 * Microglia coordinate pruning and immune responses with other glial cells.
 */
static nimcp_error_t handle_glial_sync_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("MICROGLIA", "Invalid glial sync message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_MODULE_DEBUG("MICROGLIA", "Glial sync request received from module %u", header->source_module);

    // Publish sync acknowledgment
    nimcp_error_t result = bio_router_publish_signal(g_microglia_bio_ctx,
        "microglia.sync_ack", 1.0F);

    return result;
}

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types
 * @param message_count Number of message types
 * @param user_data User-provided context (unused)
 * @return 0 on success, -1 on error
 */
static int microglia_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_MICROGLIA_ALERT:
                bio_router_register_handler(ctx, message_types[i], handle_microglia_alert_message);
                registered++;
                break;
            case BIO_MSG_MICROGLIA_PRUNE_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_microglia_prune_request_message);
                registered++;
                break;
            case BIO_MSG_METABOLIC_DEMAND:
                bio_router_register_handler(ctx, message_types[i], handle_metabolic_demand_message);
                registered++;
                break;
            case BIO_MSG_GLIAL_SYNC_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_glial_sync_message);
                registered++;
                break;
            default:
                LOG_MODULE_DEBUG("MICROGLIA", "Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_MODULE_INFO("MICROGLIA", "KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

/**
 * @brief Initialize bio-async integration for microglia module (internal implementation)
 *
 * Called once via nimcp_platform_once to ensure thread-safe initialization.
 */
static void microglia_bio_init_once_impl(void)
{
    /* Initialize unified memory manager */
    unified_mem_config_t mem_config = unified_mem_default_config();
    g_microglia_mem_mgr = unified_mem_create(&mem_config);
    if (!g_microglia_mem_mgr) {
        LOG_MODULE_ERROR("MICROGLIA", "Failed to create unified memory manager");
        g_microglia_bio_init_result = NIMCP_ERROR_MEMORY;
        return;
    }

    /* Register module with bio-router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_MICROGLIA,
        .module_name = "Microglia",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    g_microglia_bio_ctx = bio_router_register_module(&info);
    if (!g_microglia_bio_ctx) {
        LOG_MODULE_ERROR("MICROGLIA", "Failed to register with bio-router");
        unified_mem_destroy(g_microglia_mem_mgr);
        g_microglia_mem_mgr = NULL;
        g_microglia_bio_init_result = NIMCP_ERROR_INVALID_PARAM;
        return;
    }

    /* Try KG-driven wiring callback registration first */
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_MICROGLIA,
        (void*)microglia_wiring_handler_callback,
        NULL
    );

    if (result == NIMCP_SUCCESS) {
        LOG_MODULE_INFO("MICROGLIA", "KG-driven wiring callback registered successfully");
    } else {
        /* Fallback to legacy handler registration */
        LOG_MODULE_INFO("MICROGLIA", "Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_microglia_bio_ctx,
                                                  BIO_MSG_MICROGLIA_ALERT,
                                                  handle_microglia_alert_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("MICROGLIA", "Failed to register alert handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_microglia_bio_ctx,
                                                  BIO_MSG_MICROGLIA_PRUNE_REQUEST,
                                                  handle_microglia_prune_request_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("MICROGLIA", "Failed to register prune request handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_microglia_bio_ctx,
                                                  BIO_MSG_METABOLIC_DEMAND,
                                                  handle_metabolic_demand_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("MICROGLIA", "Failed to register metabolic demand handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_microglia_bio_ctx,
                                                  BIO_MSG_GLIAL_SYNC_REQUEST,
                                                  handle_glial_sync_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("MICROGLIA", "Failed to register glial sync handler: %d", result);
            goto cleanup;
        }

        LOG_MODULE_INFO("MICROGLIA", "Bio-async integration initialized with legacy handlers (4 handlers registered)");
    }

    nimcp_atomic_store_bool(&g_microglia_bio_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    g_microglia_bio_init_result = NIMCP_SUCCESS;
    return;

cleanup:
    bio_router_unregister_module(g_microglia_bio_ctx);
    g_microglia_bio_ctx = NULL;
    unified_mem_destroy(g_microglia_mem_mgr);
    g_microglia_mem_mgr = NULL;
    g_microglia_bio_init_result = result;
}

/**
 * @brief Initialize bio-async integration for microglia module (thread-safe)
 *
 * Uses nimcp_platform_once to ensure initialization runs exactly once
 * across all threads calling this function.
 */
static nimcp_error_t microglia_bio_init(void)
{
    nimcp_platform_once(&g_microglia_bio_once, microglia_bio_init_once_impl);
    return g_microglia_bio_init_result;
}

/**
 * @brief Shutdown bio-async integration
 */
static void microglia_bio_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_microglia_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    LOG_MODULE_INFO("MICROGLIA", "Shutting down bio-async integration");

    if (g_microglia_bio_ctx) {
        bio_router_unregister_module(g_microglia_bio_ctx);
        g_microglia_bio_ctx = NULL;
    }

    if (g_microglia_mem_mgr) {
        unified_mem_destroy(g_microglia_mem_mgr);
        g_microglia_mem_mgr = NULL;
    }

    nimcp_atomic_store_bool(&g_microglia_bio_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
}

//=============================================================================
// Public Bio-Async API
//=============================================================================

nimcp_result_t microglia_register_bio_handlers(void)
{
    LOG_MODULE_INFO("MICROGLIA", "Registering bio-async message handlers");
    return microglia_bio_init();
}

void microglia_unregister_bio_handlers(void)
{
    LOG_MODULE_INFO("MICROGLIA", "Unregistering bio-async message handlers");
    microglia_bio_shutdown();
}

//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Find synapse index in monitoring array
 *
 * Bounds checking: Validates mg pointer and synapses array before access.
 */
static int32_t find_synapse_index(const microglia_t* mg, uint64_t synapse_id)
{
    if (!mg || !mg->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_synapse_index: required parameter is NULL (mg, mg->synapses)");
        return -1;
    }
    if (mg->num_monitored_synapses > mg->max_monitored_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_synapse_index: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < mg->num_monitored_synapses; i++) {
        if (mg->synapses[i].synapse_id == synapse_id) {
            return (int32_t)i;
        }
    }
    return -1;  /* Not found - normal search miss */
}

/**
 * @brief Compute distance between two 3D points
 */
static inline float distance_3d(const float* p1, const float* p2)
{
    float dx = p1[0] - p2[0];
    float dy = p1[1] - p2[1];
    float dz = p1[2] - p2[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief State dynamics derivative function for RK4
 *
 * State vector: [inflammation, activation, process, energy]
 */
static void state_derivatives(const float* state, float t, void* params, float* deriv)
{
    (void)t;  // Unused
    microglia_t* mg = (microglia_t*)params;

    float inflammation = state[STATE_IDX_INFLAMMATION];
    float activation = state[STATE_IDX_ACTIVATION];
    float process = state[STATE_IDX_PROCESS];
    float energy = state[STATE_IDX_ENERGY];

    // Inflammation dynamics: driven by external input, decays naturally
    float inflammation_input = mg->inflammation_level;
    float inflammation_decay = 0.2F;
    deriv[STATE_IDX_INFLAMMATION] = inflammation_input - inflammation_decay * inflammation;

    // Activation dynamics: follows inflammation with delay
    float activation_rate = 0.5F;
    float activation_target = inflammation;  // Activation tracks inflammation
    deriv[STATE_IDX_ACTIVATION] = activation_rate * (activation_target - activation);

    // Process extension dynamics: depends on state
    float process_rate = NIMCP_MICROGLIA_PROCESS_EXTENSION_RATE / 100.0F;
    if (inflammation < NIMCP_MICROGLIA_ACTIVATION_THRESHOLD) {
        // Ramified: extend processes
        deriv[STATE_IDX_PROCESS] = process_rate * (1.0F - process);
    } else {
        // Activated/Phagocytic: retract processes
        deriv[STATE_IDX_PROCESS] = -process_rate * 2.0F * process;
    }

    // Energy dynamics: depletes with activity, regenerates at rest
    float energy_cost = 0.1F * activation;
    float energy_regen = 0.05F * (1.0F - activation);
    deriv[STATE_IDX_ENERGY] = energy_regen - energy_cost;
}

/**
 * @brief Determine state from inflammation level
 */
static microglia_state_t compute_state_from_inflammation(float inflammation)
{
    if (inflammation >= NIMCP_MICROGLIA_PHAGOCYTIC_THRESHOLD) {
        return MICROGLIA_STATE_PHAGOCYTIC;
    } else if (inflammation >= NIMCP_MICROGLIA_ACTIVATION_THRESHOLD) {
        return MICROGLIA_STATE_ACTIVATED;
    } else {
        return MICROGLIA_STATE_RAMIFIED;
    }
}

/**
 * @brief Compute effective pruning threshold with centrality protection
 */
static float compute_effective_threshold(const microglia_t* mg,
                                          const monitored_synapse_t* syn)
{
    float base_threshold = mg->pruning_threshold;

    // Centrality protection: higher centrality = higher threshold needed to prune
    if (syn->protected_by_centrality &&
        syn->centrality_score >= NIMCP_CENTRALITY_PROTECTION_MIN) {
        base_threshold *= (1.0F + syn->centrality_score * NIMCP_CENTRALITY_PROTECTION_FACTOR);
    }

    // C3-tagged synapses are easier to prune (lower effective threshold)
    if (syn->complement.tag == COMPLEMENT_C3) {
        base_threshold *= 1.5F;  // Raise threshold meaning lower activity is OK to prune
    }

    return base_threshold;
}

//=============================================================================
// CREATION & DESTRUCTION
//=============================================================================

microglia_t* microglia_create(uint32_t id, float x, float y, float z,
                               float surveillance_radius)
{
    /* Initialize bio-async on first create (thread-safe via nimcp_platform_once) */
    nimcp_error_t init_result = microglia_bio_init();
    if (init_result != NIMCP_SUCCESS) {
        LOG_MODULE_WARN("MICROGLIA", "Bio-async init failed: %d (continuing anyway)", init_result);
    }

    if (surveillance_radius <= 0.0F) {
        LOG_MODULE_ERROR("MICROGLIA", "Invalid surveillance radius: %.2f", surveillance_radius);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "microglia_create: invalid surveillance_radius");
        return NULL;
    }

    microglia_t* mg = (microglia_t*)nimcp_malloc(sizeof(microglia_t));
    NIMCP_API_CHECK_ALLOC(mg, "microglia_create: Failed to allocate microglia structure");

    memset(mg, 0, sizeof(microglia_t));

    // Basic properties
    mg->id = id;
    mg->position[0] = x;
    mg->position[1] = y;
    mg->position[2] = z;
    mg->surveillance_radius = surveillance_radius;
    mg->process_extension = 1.0F;  // Start fully extended (ramified)

    // State dynamics
    mg->state = MICROGLIA_STATE_RAMIFIED;
    mg->inflammation_level = 0.0F;
    mg->state_variables[STATE_IDX_INFLAMMATION] = 0.0F;
    mg->state_variables[STATE_IDX_ACTIVATION] = 0.0F;
    mg->state_variables[STATE_IDX_PROCESS] = 1.0F;
    mg->state_variables[STATE_IDX_ENERGY] = 1.0F;

    // Cytokine state
    memset(&mg->cytokines, 0, sizeof(microglia_cytokine_state_t));
    mg->cytokines.last_update_time = nimcp_time_monotonic_us();

    // Allocate enhanced synapse array
    mg->max_monitored_synapses = NIMCP_MICROGLIA_DEFAULT_CAPACITY;
    mg->num_monitored_synapses = 0;

    mg->synapses = (monitored_synapse_t*)nimcp_malloc(
        mg->max_monitored_synapses * sizeof(monitored_synapse_t));
    if (!mg->synapses) {
        nimcp_free(mg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "microglia_create: mg->synapses is NULL");
        return NULL;
    }
    memset(mg->synapses, 0, mg->max_monitored_synapses * sizeof(monitored_synapse_t));

    // Legacy arrays for backward compatibility (G7: synapse_id widened to uint64)
    mg->monitored_synapse_ids = (uint64_t*)nimcp_malloc(
        mg->max_monitored_synapses * sizeof(uint64_t));
    mg->synapse_activity_scores = (float*)nimcp_malloc(
        mg->max_monitored_synapses * sizeof(float));
    mg->last_activity_times = (uint64_t*)nimcp_malloc(
        mg->max_monitored_synapses * sizeof(uint64_t));

    if (!mg->monitored_synapse_ids || !mg->synapse_activity_scores ||
        !mg->last_activity_times) {
        microglia_destroy(mg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_create: operation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < mg->max_monitored_synapses; i++) {
        mg->monitored_synapse_ids[i] = UINT64_MAX;
        mg->synapse_activity_scores[i] = 0.0F;
        mg->last_activity_times[i] = 0;
    }

    // Pruning parameters
    mg->pruning_threshold = NIMCP_MICROGLIA_PRUNING_THRESHOLD;
    mg->pruning_rate = NIMCP_MICROGLIA_PRUNING_RATE;
    mg->last_pruning_time = nimcp_time_monotonic_us();
    mg->total_synapses_pruned = 0;
    mg->total_c1q_tags = 0;
    mg->total_c3_conversions = 0;
    mg->protected_from_pruning = 0;

    // Initialize lock
    nimcp_spinlock_init(&mg->lock);

    // Initialize CoW fields (Phase 1.5+)
    mg->cow_ref_count = 1;
    mg->cow_modified = false;
    mg->cow_original = NULL;

    return mg;
}

void microglia_destroy(microglia_t* mg)
{
    if (!mg) return;

    if (mg->synapses) {
        nimcp_free(mg->synapses);
    }
    if (mg->monitored_synapse_ids) {
        nimcp_free(mg->monitored_synapse_ids);
    }
    if (mg->synapse_activity_scores) {
        nimcp_free(mg->synapse_activity_scores);
    }
    if (mg->last_activity_times) {
        nimcp_free(mg->last_activity_times);
    }

    nimcp_free(mg);
}

microglia_network_config_t microglia_network_default_config(void)
{
    microglia_network_config_t config = {
        .capacity = 100,
        .pruning_threshold = NIMCP_MICROGLIA_PRUNING_THRESHOLD,
        .pruning_rate = NIMCP_MICROGLIA_PRUNING_RATE,
        .surveillance_radius = NIMCP_MICROGLIA_SURVEILLANCE_RADIUS_UM,
        .enable_centrality_protection = true,
        .enable_complement_cascade = true,
        .enable_cytokine_signaling = true,
        .enable_state_dynamics = true,
        .filter_cutoff_hz = 1.0F
    };
    return config;
}

microglia_network_t* microglia_network_create_enhanced(
    const microglia_network_config_t* config)
{
    if (!config || config->capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_network_create_enhanced: config is NULL");
        return NULL;
    }

    microglia_network_t* network = (microglia_network_t*)nimcp_malloc(
        sizeof(microglia_network_t));
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network is NULL");

        return NULL;
    }

    memset(network, 0, sizeof(microglia_network_t));

    network->capacity = config->capacity;
    network->num_microglia = 0;

    network->microglia = (microglia_t**)nimcp_malloc(
        config->capacity * sizeof(microglia_t*));
    if (!network->microglia) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "microglia_network_create_enhanced: network->microglia is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < config->capacity; i++) {
        network->microglia[i] = NULL;
    }

    // Create KD-trees for spatial indexing
    network->microglia_tree = kdtree_create();
    network->synapse_tree = kdtree_create();
    network->spatial_index_valid = false;

    // Global parameters
    network->global_pruning_threshold = config->pruning_threshold;
    network->min_activity_window_ms = NIMCP_MICROGLIA_MIN_ACTIVITY_WINDOW_MS;
    network->global_inflammation = 0.0F;

    // Activity filter
    network->filter_cutoff_hz = config->filter_cutoff_hz;
    // alpha = 2πf_c / (2πf_c + f_s), simplified for ~1000Hz sample rate
    network->filter_alpha = 2.0F * M_PI * config->filter_cutoff_hz /
                            (2.0F * M_PI * config->filter_cutoff_hz + 1000.0F);
    if (network->filter_alpha < 0.01F) network->filter_alpha = 0.01F;
    if (network->filter_alpha > 1.0F) network->filter_alpha = 1.0F;

    // Centrality (will be populated later)
    network->synapse_centrality = NULL;
    network->num_centrality_scores = 0;
    network->centrality_valid = false;

    // Cytokine field (placeholder - could be 3D grid)
    network->global_cytokine_field = NULL;
    network->cytokine_field_size = 0;

    nimcp_mutex_init(&network->lock, NULL);

    // Create synapse memory pool (Phase 1.5+)
    network->synapse_pool = microglia_synapse_pool_create(
        NIMCP_MICROGLIA_SYNAPSE_POOL_SIZE);
    // Pool is optional - continue even if creation fails

    return network;
}

microglia_network_t* microglia_network_create(uint32_t capacity)
{
    microglia_network_config_t config = microglia_network_default_config();
    config.capacity = capacity;
    return microglia_network_create_enhanced(&config);
}

void microglia_network_destroy(microglia_network_t* network)
{
    if (!network) return;

    // Destroy all microglia
    for (uint32_t i = 0; i < network->num_microglia; i++) {
        if (network->microglia[i]) {
            microglia_destroy(network->microglia[i]);
        }
    }

    if (network->microglia) {
        nimcp_free(network->microglia);
    }

    // Destroy KD-trees
    if (network->microglia_tree) {
        kdtree_destroy(network->microglia_tree);
    }
    if (network->synapse_tree) {
        kdtree_destroy(network->synapse_tree);
    }

    // Free centrality scores
    if (network->synapse_centrality) {
        nimcp_free(network->synapse_centrality);
    }

    // Free cytokine field
    if (network->global_cytokine_field) {
        nimcp_free(network->global_cytokine_field);
    }

    // Destroy synapse memory pool (Phase 1.5+)
    if (network->synapse_pool) {
        microglia_synapse_pool_destroy(network->synapse_pool);
    }

    nimcp_mutex_destroy(&network->lock);
    nimcp_free(network);
}

//=============================================================================
// SYNAPSE MONITORING
//=============================================================================

nimcp_result_t microglia_monitor_synapse_at(microglia_t* mg, uint64_t synapse_id,
                                             float x, float y, float z)
{
    NIMCP_CHECK_THROW(mg, NIMCP_ERROR_INVALID_PARAM, "mg is NULL");

    nimcp_spinlock_lock(&mg->lock);

    // Check for duplicate
    int32_t existing_idx = find_synapse_index(mg, synapse_id);
    if (existing_idx >= 0) {
        // Update position
        mg->synapses[existing_idx].position[0] = x;
        mg->synapses[existing_idx].position[1] = y;
        mg->synapses[existing_idx].position[2] = z;
        nimcp_spinlock_unlock(&mg->lock);
        return NIMCP_SUCCESS;
    }

    // Check capacity
    if (mg->num_monitored_synapses >= mg->max_monitored_synapses) {
        nimcp_spinlock_unlock(&mg->lock);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Add synapse
    uint32_t idx = mg->num_monitored_synapses;
    monitored_synapse_t* syn = &mg->synapses[idx];

    syn->synapse_id = synapse_id;
    syn->position[0] = x;
    syn->position[1] = y;
    syn->position[2] = z;
    syn->activity_score = 0.0F;
    syn->filtered_activity = 0.0F;
    syn->last_activity_time = nimcp_time_monotonic_us();
    syn->complement.tag = COMPLEMENT_NONE;
    syn->complement.tag_strength = 0.0F;
    syn->complement.tag_time = 0;
    syn->centrality_score = 0.0F;
    syn->protected_by_centrality = false;

    // Update legacy arrays
    mg->monitored_synapse_ids[idx] = synapse_id;
    mg->synapse_activity_scores[idx] = 0.0F;
    mg->last_activity_times[idx] = syn->last_activity_time;

    mg->num_monitored_synapses++;

    nimcp_spinlock_unlock(&mg->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t microglia_monitor_synapse(microglia_t* mg, uint64_t synapse_id)
{
    // Default position at microglia location
    NIMCP_CHECK_THROW(mg, NIMCP_ERROR_INVALID_PARAM, "mg is NULL");
    return microglia_monitor_synapse_at(mg, synapse_id,
                                         mg->position[0], mg->position[1], mg->position[2]);
}

void microglia_track_synapse_activity(microglia_t* mg, uint64_t synapse_id,
                                       float activity, uint64_t timestamp)
{
    if (!mg) return;

    activity = nimcp_clampf(activity, 0.0F, 10.0F);

    nimcp_spinlock_lock(&mg->lock);

    int32_t idx = find_synapse_index(mg, synapse_id);
    if (idx >= 0) {
        monitored_synapse_t* syn = &mg->synapses[idx];

        // Apply low-pass filter to activity
        float alpha = DEFAULT_FILTER_ALPHA;
        syn->filtered_activity = alpha * activity + (1.0F - alpha) * syn->filtered_activity;

        // Update raw activity score with EMA
        if (syn->activity_score < 0.001F) {
            syn->activity_score = activity;
        } else {
            float ema_alpha = 0.3F;
            syn->activity_score = ema_alpha * activity + (1.0F - ema_alpha) * syn->activity_score;
        }

        syn->last_activity_time = timestamp;

        // Update legacy arrays
        mg->synapse_activity_scores[idx] = syn->activity_score;
        mg->last_activity_times[idx] = timestamp;
    }

    nimcp_spinlock_unlock(&mg->lock);
}

void microglia_update_activity_scores(microglia_t* mg, uint64_t current_time)
{
    if (!mg) return;

    nimcp_spinlock_lock(&mg->lock);

    float tau_us = NIMCP_MICROGLIA_ACTIVITY_DECAY_TAU_S * 1000000.0F;

    for (uint32_t i = 0; i < mg->num_monitored_synapses; i++) {
        monitored_synapse_t* syn = &mg->synapses[i];
        uint64_t dt_us = current_time - syn->last_activity_time;
        float dt_s = (float)dt_us / 1000000.0F;

        // Exponential decay
        float decay_factor = expf(-dt_s * 1000000.0F / tau_us);
        syn->activity_score *= decay_factor;
        syn->filtered_activity *= decay_factor;

        // Clamp to zero
        if (syn->activity_score < 0.001F) {
            syn->activity_score = 0.0F;
        }
        if (syn->filtered_activity < 0.001F) {
            syn->filtered_activity = 0.0F;
        }

        // Update legacy arrays
        mg->synapse_activity_scores[i] = syn->activity_score;
    }

    nimcp_spinlock_unlock(&mg->lock);
}

float microglia_get_synapse_activity_score(microglia_t* mg, uint64_t synapse_id)
{
    if (!mg) return 0.0F;

    nimcp_spinlock_lock(&mg->lock);

    int32_t idx = find_synapse_index(mg, synapse_id);
    float score = (idx >= 0) ? mg->synapses[idx].activity_score : 0.0F;

    nimcp_spinlock_unlock(&mg->lock);
    return score;
}

void microglia_set_synapse_centrality(microglia_t* mg, uint64_t synapse_id,
                                       float centrality)
{
    if (!mg) return;

    centrality = nimcp_clampf(centrality, 0.0F, 1.0F);

    nimcp_spinlock_lock(&mg->lock);

    int32_t idx = find_synapse_index(mg, synapse_id);
    if (idx >= 0) {
        mg->synapses[idx].centrality_score = centrality;
        mg->synapses[idx].protected_by_centrality =
            (centrality >= NIMCP_CENTRALITY_PROTECTION_MIN);
    }

    nimcp_spinlock_unlock(&mg->lock);
}

//=============================================================================
// STATE DYNAMICS (RK4 ODE)
//=============================================================================

void microglia_update_state_dynamics(microglia_t* mg, float dt)
{
    if (!mg || dt <= 0.0F) return;

    // Process pending bio-async messages before state update
    if (nimcp_atomic_load_bool(&g_microglia_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) && g_microglia_bio_ctx) {
        bio_router_process_inbox(g_microglia_bio_ctx, 5);  // Process up to 5 messages
    }

    nimcp_spinlock_lock(&mg->lock);

    // Use RK4 integration for state dynamics
    float k1[4], k2[4], k3[4], k4[4];
    float temp_state[4];

    // k1 = f(state, t)
    state_derivatives(mg->state_variables, 0.0F, mg, k1);

    // k2 = f(state + dt/2 * k1, t + dt/2)
    for (int i = 0; i < 4; i++) {
        temp_state[i] = mg->state_variables[i] + 0.5F * dt * k1[i];
    }
    state_derivatives(temp_state, dt * 0.5F, mg, k2);

    // k3 = f(state + dt/2 * k2, t + dt/2)
    for (int i = 0; i < 4; i++) {
        temp_state[i] = mg->state_variables[i] + 0.5F * dt * k2[i];
    }
    state_derivatives(temp_state, dt * 0.5F, mg, k3);

    // k4 = f(state + dt * k3, t + dt)
    for (int i = 0; i < 4; i++) {
        temp_state[i] = mg->state_variables[i] + dt * k3[i];
    }
    state_derivatives(temp_state, dt, mg, k4);

    // Update state: state_new = state + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
    for (int i = 0; i < 4; i++) {
        mg->state_variables[i] += dt / 6.0F * (k1[i] + 2.0F*k2[i] + 2.0F*k3[i] + k4[i]);
    }

    // Clamp values
    mg->state_variables[STATE_IDX_INFLAMMATION] =
        nimcp_clampf(mg->state_variables[STATE_IDX_INFLAMMATION], 0.0F, 1.0F);
    mg->state_variables[STATE_IDX_ACTIVATION] =
        nimcp_clampf(mg->state_variables[STATE_IDX_ACTIVATION], 0.0F, 1.0F);
    mg->state_variables[STATE_IDX_PROCESS] =
        nimcp_clampf(mg->state_variables[STATE_IDX_PROCESS], 0.0F, 1.0F);
    mg->state_variables[STATE_IDX_ENERGY] =
        nimcp_clampf(mg->state_variables[STATE_IDX_ENERGY], 0.0F, 1.0F);

    // Update derived values
    mg->process_extension = mg->state_variables[STATE_IDX_PROCESS];
    mg->state = compute_state_from_inflammation(mg->state_variables[STATE_IDX_INFLAMMATION]);

    nimcp_spinlock_unlock(&mg->lock);
}

microglia_state_t microglia_get_state(const microglia_t* mg)
{
    if (!mg) return MICROGLIA_STATE_RAMIFIED;
    return mg->state;
}

void microglia_set_inflammation(microglia_t* mg, float inflammation)
{
    if (!mg) return;

    nimcp_spinlock_lock(&mg->lock);
    mg->inflammation_level = nimcp_clampf(inflammation, 0.0F, 1.0F);
    nimcp_spinlock_unlock(&mg->lock);

    // Publish inflammation via NOREPINEPHRINE (alerting) channel
    if (nimcp_atomic_load_bool(&g_microglia_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) && g_microglia_bio_ctx) {
        bio_router_publish_signal(g_microglia_bio_ctx, "microglia.inflammation", mg->inflammation_level);
        LOG_MODULE_DEBUG("MICROGLIA", "Published inflammation level: %.3f", mg->inflammation_level);
    }
}

float microglia_get_process_extension(const microglia_t* mg)
{
    if (!mg) return 0.0F;
    return mg->process_extension;
}

//=============================================================================
// COMPLEMENT CASCADE
//=============================================================================

uint32_t microglia_apply_complement_tags(microglia_t* mg, uint64_t current_time)
{
    if (!mg) return 0;

    nimcp_spinlock_lock(&mg->lock);

    uint32_t newly_tagged = 0;
    float c1q_threshold = NIMCP_COMPLEMENT_C1Q_THRESHOLD;
    uint64_t conversion_time_us = (uint64_t)(C1Q_TO_C3_CONVERSION_TIME_S * 1000000.0F);

    for (uint32_t i = 0; i < mg->num_monitored_synapses; i++) {
        monitored_synapse_t* syn = &mg->synapses[i];

        // Only tag low-activity synapses
        if (syn->filtered_activity < c1q_threshold) {
            if (syn->complement.tag == COMPLEMENT_NONE) {
                // Apply C1q tag
                syn->complement.tag = COMPLEMENT_C1Q;
                syn->complement.tag_strength = 1.0F - (syn->filtered_activity / c1q_threshold);
                syn->complement.tag_time = current_time;
                mg->total_c1q_tags++;
                newly_tagged++;
            } else if (syn->complement.tag == COMPLEMENT_C1Q) {
                // Check for C3 conversion
                uint64_t tag_age = current_time - syn->complement.tag_time;
                if (tag_age >= conversion_time_us) {
                    syn->complement.tag = COMPLEMENT_C3;
                    syn->complement.tag_strength = 1.0F;
                    mg->total_c3_conversions++;
                }
            }
        } else {
            // Activity recovered - decay tag
            if (syn->complement.tag != COMPLEMENT_NONE) {
                syn->complement.tag_strength -= 0.1F;
                if (syn->complement.tag_strength <= 0.0F) {
                    syn->complement.tag = COMPLEMENT_NONE;
                    syn->complement.tag_strength = 0.0F;
                }
            }
        }
    }

    nimcp_spinlock_unlock(&mg->lock);
    return newly_tagged;
}

complement_tag_t microglia_get_complement_tag(const microglia_t* mg, uint64_t synapse_id)
{
    if (!mg) return COMPLEMENT_NONE;

    // Note: Not locking for const read - may need adjustment for strict thread safety
    for (uint32_t i = 0; i < mg->num_monitored_synapses; i++) {
        if (mg->synapses[i].synapse_id == synapse_id) {
            return mg->synapses[i].complement.tag;
        }
    }
    return COMPLEMENT_NONE;
}

void microglia_decay_complement_tags(microglia_t* mg, float dt)
{
    if (!mg || dt <= 0.0F) return;

    nimcp_spinlock_lock(&mg->lock);

    float decay = NIMCP_COMPLEMENT_DECAY_RATE * dt;

    for (uint32_t i = 0; i < mg->num_monitored_synapses; i++) {
        monitored_synapse_t* syn = &mg->synapses[i];
        if (syn->complement.tag != COMPLEMENT_NONE) {
            syn->complement.tag_strength -= decay;
            if (syn->complement.tag_strength <= 0.0F) {
                syn->complement.tag = COMPLEMENT_NONE;
                syn->complement.tag_strength = 0.0F;
            }
        }
    }

    nimcp_spinlock_unlock(&mg->lock);
}

//=============================================================================
// CYTOKINE SIGNALING
//=============================================================================

void microglia_update_cytokines(microglia_t* mg, float dt)
{
    if (!mg || dt <= 0.0F) return;

    nimcp_spinlock_lock(&mg->lock);

    // Production rates based on state
    float production[NIMCP_CYTOKINE_COUNT] = {0};

    switch (mg->state) {
        case MICROGLIA_STATE_RAMIFIED:
            // Low baseline production
            production[CYTOKINE_IL1B] = 0.01F;
            production[CYTOKINE_TNFA] = 0.01F;
            production[CYTOKINE_IL6] = 0.01F;
            production[CYTOKINE_IL10] = 0.02F;
            production[CYTOKINE_TGFB] = 0.02F;
            break;

        case MICROGLIA_STATE_ACTIVATED:
            // High pro-inflammatory
            production[CYTOKINE_IL1B] = 0.5F;
            production[CYTOKINE_TNFA] = 0.4F;
            production[CYTOKINE_IL6] = 0.3F;
            production[CYTOKINE_IL10] = 0.05F;
            production[CYTOKINE_TGFB] = 0.05F;
            break;

        case MICROGLIA_STATE_PHAGOCYTIC:
            // Resolution phase - anti-inflammatory
            production[CYTOKINE_IL1B] = 0.1F;
            production[CYTOKINE_TNFA] = 0.1F;
            production[CYTOKINE_IL6] = 0.1F;
            production[CYTOKINE_IL10] = 0.5F;
            production[CYTOKINE_TGFB] = 0.4F;
            break;
    }

    // Update concentrations: dC/dt = production - decay * C
    for (int i = 0; i < NIMCP_CYTOKINE_COUNT; i++) {
        mg->cytokines.production_rates[i] = production[i];
        float dC = production[i] - NIMCP_CYTOKINE_DECAY_RATE * mg->cytokines.concentrations[i];
        mg->cytokines.concentrations[i] += dC * dt;
        mg->cytokines.concentrations[i] = nimcp_clampf(mg->cytokines.concentrations[i],
                                                   0.0F, NIMCP_CYTOKINE_MAX_CONCENTRATION);
    }

    mg->cytokines.last_update_time = nimcp_time_monotonic_us();

    nimcp_spinlock_unlock(&mg->lock);
}

float microglia_get_cytokine(const microglia_t* mg, cytokine_type_t type)
{
    if (!mg || type >= NIMCP_CYTOKINE_COUNT) return 0.0F;
    return mg->cytokines.concentrations[type];
}

void microglia_add_cytokine(microglia_t* mg, cytokine_type_t type, float amount)
{
    if (!mg || type >= NIMCP_CYTOKINE_COUNT) return;

    nimcp_spinlock_lock(&mg->lock);
    mg->cytokines.concentrations[type] = nimcp_clampf(
        mg->cytokines.concentrations[type] + amount,
        0.0F, NIMCP_CYTOKINE_MAX_CONCENTRATION);
    nimcp_spinlock_unlock(&mg->lock);
}

float microglia_get_net_inflammation(const microglia_t* mg)
{
    if (!mg) return 0.0F;

    /* Acquire spinlock before reading cytokine concentrations */
    nimcp_spinlock_lock((nimcp_spinlock_t*)&mg->lock);

    float pro = mg->cytokines.concentrations[CYTOKINE_IL1B] +
                mg->cytokines.concentrations[CYTOKINE_TNFA] +
                mg->cytokines.concentrations[CYTOKINE_IL6];

    float anti = mg->cytokines.concentrations[CYTOKINE_IL10] +
                 mg->cytokines.concentrations[CYTOKINE_TGFB];

    nimcp_spinlock_unlock((nimcp_spinlock_t*)&mg->lock);

    return pro - anti;
}

//=============================================================================
// PRUNING (Enhanced with Centrality Protection)
//=============================================================================

uint32_t microglia_identify_weak_synapses(microglia_t* mg,
                                           uint32_t* weak_synapse_ids,
                                           uint32_t max_count)
{
    if (!mg || !weak_synapse_ids || max_count == 0) return 0;

    nimcp_spinlock_lock(&mg->lock);

    uint32_t num_weak = 0;

    for (uint32_t i = 0; i < mg->num_monitored_synapses && num_weak < max_count; i++) {
        monitored_synapse_t* syn = &mg->synapses[i];
        float effective_threshold = compute_effective_threshold(mg, syn);

        // Use filtered activity for more stable decisions
        if (syn->filtered_activity < effective_threshold) {
            // Check centrality protection
            if (syn->protected_by_centrality &&
                syn->centrality_score >= NIMCP_CENTRALITY_PROTECTION_MIN) {
                // Only prune if C3 tagged (overrides protection)
                if (syn->complement.tag != COMPLEMENT_C3) {
                    mg->protected_from_pruning++;
                    continue;
                }
            }

            weak_synapse_ids[num_weak] = syn->synapse_id;
            num_weak++;
        }
    }

    nimcp_spinlock_unlock(&mg->lock);
    return num_weak;
}

uint32_t microglia_prune_weak_synapses(microglia_t* mg)
{
    if (!mg || mg->num_monitored_synapses == 0) return 0;

    nimcp_spinlock_lock(&mg->lock);

    uint32_t num_pruned = 0;
    uint32_t max_prune = (uint32_t)mg->pruning_rate;

    // Prioritize C3-tagged synapses
    uint32_t write_idx = 0;

    // First pass: prune C3-tagged weak synapses
    for (uint32_t read_idx = 0; read_idx < mg->num_monitored_synapses; read_idx++) {
        monitored_synapse_t* syn = &mg->synapses[read_idx];
        float effective_threshold = compute_effective_threshold(mg, syn);

        bool is_weak = syn->filtered_activity < effective_threshold;
        bool should_prune = is_weak && (num_pruned < max_prune);

        // Respect centrality protection unless C3-tagged
        if (should_prune && syn->protected_by_centrality &&
            syn->centrality_score >= NIMCP_CENTRALITY_PROTECTION_MIN &&
            syn->complement.tag != COMPLEMENT_C3) {
            should_prune = false;
            mg->protected_from_pruning++;
        }

        if (should_prune) {
            num_pruned++;
            // Don't copy - effectively removes
        } else {
            // Keep this synapse
            if (write_idx != read_idx) {
                mg->synapses[write_idx] = mg->synapses[read_idx];
                mg->monitored_synapse_ids[write_idx] = mg->monitored_synapse_ids[read_idx];
                mg->synapse_activity_scores[write_idx] = mg->synapse_activity_scores[read_idx];
                mg->last_activity_times[write_idx] = mg->last_activity_times[read_idx];
            }
            write_idx++;
        }
    }

    mg->num_monitored_synapses = write_idx;
    mg->total_synapses_pruned += num_pruned;
    mg->last_pruning_time = nimcp_time_monotonic_us();

    nimcp_spinlock_unlock(&mg->lock);

    // Publish pruning event via NOREPINEPHRINE (alerting) channel
    if (num_pruned > 0 && nimcp_atomic_load_bool(&g_microglia_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) && g_microglia_bio_ctx) {
        bio_router_publish_signal(g_microglia_bio_ctx, "microglia.pruning", (float)num_pruned);
        LOG_MODULE_INFO("MICROGLIA", "Pruned %u synapses - published via bio-async", num_pruned);
    }

    return num_pruned;
}

bool microglia_should_prune_synapse(const microglia_t* mg, uint64_t synapse_id)
{
    if (!mg) {
        return false;
    }

    for (uint32_t i = 0; i < mg->num_monitored_synapses; i++) {
        if (mg->synapses[i].synapse_id == synapse_id) {
            const monitored_synapse_t* syn = &mg->synapses[i];
            float effective_threshold = compute_effective_threshold(mg, syn);

            if (syn->filtered_activity >= effective_threshold) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "microglia_should_prune_synapse: capacity exceeded");
                return false;  // Activity above threshold
            }

            // Check protection
            if (syn->protected_by_centrality &&
                syn->centrality_score >= NIMCP_CENTRALITY_PROTECTION_MIN &&
                syn->complement.tag != COMPLEMENT_C3) {
                return false;  // Protected by centrality - normal decision
            }

            return true;  // Should prune
        }
    }

    return false;  // Synapse not found - don't prune
}

//=============================================================================
// NETWORK OPERATIONS
//=============================================================================

nimcp_result_t microglia_network_add(microglia_network_t* network, microglia_t* mg)
{
    NIMCP_CHECK_THROW(network && mg, NIMCP_ERROR_INVALID_PARAM, "network or mg is NULL");
    NIMCP_CHECK_THROW(network->microglia, NIMCP_ERROR_INVALID_PARAM, "network->microglia is NULL");

    nimcp_mutex_lock(&network->lock);

    /* Bounds check: ensure we don't exceed allocated capacity */
    if (network->num_microglia >= network->capacity) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    network->microglia[network->num_microglia] = mg;
    network->num_microglia++;
    network->spatial_index_valid = false;  /* Need to rebuild KD-tree */
    network->synapse_changes_since_rebuild++;  /* Track changes for lazy rebuild */

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

void microglia_network_rebuild_spatial_index(microglia_network_t* network)
{
    if (!network || network->num_microglia == 0) return;
    if (!network->microglia) return;
    /* Bounds validation: num_microglia should not exceed capacity */
    if (network->num_microglia > network->capacity) return;

    nimcp_mutex_lock(&network->lock);

    /* Rebuild microglia KD-tree */
    if (network->microglia_tree) {
        kdtree_destroy(network->microglia_tree);
    }
    network->microglia_tree = kdtree_create();

    if (network->microglia_tree && network->num_microglia > 0) {
        kdtree_point_t* points = (kdtree_point_t*)nimcp_malloc(
            network->num_microglia * sizeof(kdtree_point_t));
        void** user_data = (void**)nimcp_malloc(
            network->num_microglia * sizeof(void*));

        if (points && user_data) {
            for (uint32_t i = 0; i < network->num_microglia; i++) {
                microglia_t* mg = network->microglia[i];
                points[i][0] = mg->position[0];
                points[i][1] = mg->position[1];
                points[i][2] = mg->position[2];
                user_data[i] = mg;
            }

            kdtree_build(network->microglia_tree, points, user_data, network->num_microglia);
        }

        if (points) nimcp_free(points);
        if (user_data) nimcp_free(user_data);
    }

    network->spatial_index_valid = true;
    network->synapse_changes_since_rebuild = 0;  /* Reset change counter after rebuild */

    /* Update total synapse count for lazy rebuild threshold */
    uint32_t total_synapses = 0;
    for (uint32_t i = 0; i < network->num_microglia; i++) {
        if (network->microglia[i]) {
            total_synapses += network->microglia[i]->num_monitored_synapses;
        }
    }
    network->total_monitored_synapse_count = total_synapses;

    nimcp_mutex_unlock(&network->lock);
}

void microglia_network_update_centrality(microglia_network_t* network, void* synapse_graph)
{
    if (!network) return;

    nimcp_mutex_lock(&network->lock);

    // This would integrate with nimcp_centrality.h
    // For now, we'll set a placeholder that can be populated externally
    // In full integration, this would call:
    // NimcpCentralityScores* scores = nimcp_betweenness_centrality((NimcpGraph*)synapse_graph);

    // Mark centrality as needing external update
    network->centrality_valid = (synapse_graph != NULL);

    nimcp_mutex_unlock(&network->lock);
}

void microglia_network_step(microglia_network_t* network, uint64_t current_time)
{
    if (!network) return;
    if (!network->microglia) return;
    /* Bounds validation: num_microglia should not exceed capacity */
    if (network->num_microglia > network->capacity) return;

    nimcp_mutex_lock(&network->lock);

    // Compute dt from last step (default to 1ms if first step)
    // THREAD-SAFETY FIX: Use per-network last_step_time instead of static variable
    // to avoid shared state across network instances and enable thread-safe operation.
    float dt_s;
    if (network->last_step_time == 0) {
        dt_s = 0.001F;
    } else {
        dt_s = (float)(current_time - network->last_step_time) / 1000000.0F;
        if (dt_s > 0.1F) dt_s = 0.1F;  // Cap at 100ms
        if (dt_s < 0.0001F) dt_s = 0.0001F;  // Min 0.1ms
    }
    network->last_step_time = current_time;

    /* Phase 8: Send heartbeat at start of network step */
    microglia_heartbeat("microglia_network_step", 0.0f);

    // Process each microglia
    for (uint32_t i = 0; i < network->num_microglia; i++) {
        microglia_t* mg = network->microglia[i];
        if (!mg) continue;

        // 1. Update state dynamics (RK4)
        microglia_update_state_dynamics(mg, dt_s);

        // 2. Update cytokines
        microglia_update_cytokines(mg, dt_s);

        // 3. Apply complement tagging
        microglia_apply_complement_tags(mg, current_time);

        // 4. Decay complement tags
        microglia_decay_complement_tags(mg, dt_s);

        // 5. Update activity scores
        microglia_update_activity_scores(mg, current_time);

        // 6. Prune weak synapses
        microglia_prune_weak_synapses(mg);

        /* Phase 8: Send heartbeat for progress tracking in large networks */
        if ((i & 0xFF) == 0 && network->num_microglia > 256) {
            microglia_heartbeat("microglia_network_step",
                               (float)(i + 1) / (float)network->num_microglia);
        }
    }

    // Diffuse cytokines between nearby microglia
    microglia_network_diffuse_cytokines(network, dt_s);

    // Lazy KD-tree rebuild: only rebuild when synapse changes exceed 10% threshold
    // This avoids expensive O(N log N) rebuilds on every minor change
    if (!network->spatial_index_valid) {
        uint32_t threshold = network->total_monitored_synapse_count / 10;
        if (threshold < 10) threshold = 10;  // Minimum threshold
        if (network->synapse_changes_since_rebuild >= threshold) {
            nimcp_mutex_unlock(&network->lock);
            microglia_network_rebuild_spatial_index(network);
        } else {
            nimcp_mutex_unlock(&network->lock);
        }
    } else {
        nimcp_mutex_unlock(&network->lock);
    }
    return;
}

microglia_t* microglia_network_find_by_synapse(microglia_network_t* network,
                                                uint64_t synapse_id)
{
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    nimcp_mutex_lock(&network->lock);

    for (uint32_t i = 0; i < network->num_microglia; i++) {
        microglia_t* mg = network->microglia[i];
        if (!mg) continue;

        if (find_synapse_index(mg, synapse_id) >= 0) {
            nimcp_mutex_unlock(&network->lock);
            return mg;
        }
    }

    nimcp_mutex_unlock(&network->lock);
    return NULL;  /* Not found - normal search miss */
}

microglia_t* microglia_network_find_nearest(microglia_network_t* network,
                                             float x, float y, float z)
{
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    nimcp_mutex_lock(&network->lock);

    // Use KD-tree if valid
    if (network->spatial_index_valid && network->microglia_tree) {
        kdtree_point_t query = {x, y, z};
        float dist_sq = 0.0F;

        void* nearest = kdtree_nearest(network->microglia_tree, query, &dist_sq);
        if (nearest) {
            nimcp_mutex_unlock(&network->lock);
            return (microglia_t*)nearest;
        }
    }

    // Fallback to linear search
    microglia_t* nearest = NULL;
    float min_dist = FLT_MAX;
    float query_pos[3] = {x, y, z};

    for (uint32_t i = 0; i < network->num_microglia; i++) {
        microglia_t* mg = network->microglia[i];
        if (!mg) continue;

        float dist = distance_3d(query_pos, mg->position);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = mg;
        }
    }

    nimcp_mutex_unlock(&network->lock);
    return nearest;
}

uint32_t microglia_network_find_in_radius(microglia_network_t* network,
                                           float x, float y, float z,
                                           float radius,
                                           microglia_t** results,
                                           uint32_t max_results)
{
    if (!network || !results || max_results == 0) return 0;

    nimcp_mutex_lock(&network->lock);

    uint32_t count = 0;
    float query_pos[3] = {x, y, z};
    float radius_sq = radius * radius;

    for (uint32_t i = 0; i < network->num_microglia && count < max_results; i++) {
        microglia_t* mg = network->microglia[i];
        if (!mg) continue;

        float dx = mg->position[0] - query_pos[0];
        float dy = mg->position[1] - query_pos[1];
        float dz = mg->position[2] - query_pos[2];
        float dist_sq = dx*dx + dy*dy + dz*dz;

        if (dist_sq <= radius_sq) {
            results[count++] = mg;
        }
    }

    nimcp_mutex_unlock(&network->lock);
    return count;
}

void microglia_network_diffuse_cytokines(microglia_network_t* network, float dt)
{
    if (!network || dt <= 0.0F || network->num_microglia < 2) return;
    if (!network->microglia) return;
    /* Bounds validation: num_microglia should not exceed capacity */
    if (network->num_microglia > network->capacity) return;

    /* Simple diffusion: each microglia shares cytokines with nearby neighbors */
    float diffusion_radius = 200.0F;  // µm
    float diffusion_rate = NIMCP_CYTOKINE_DIFFUSION_COEFF * dt / (diffusion_radius * diffusion_radius);

    // For each pair of nearby microglia, exchange cytokines
    for (uint32_t i = 0; i < network->num_microglia; i++) {
        microglia_t* mg1 = network->microglia[i];
        if (!mg1) continue;

        for (uint32_t j = i + 1; j < network->num_microglia; j++) {
            microglia_t* mg2 = network->microglia[j];
            if (!mg2) continue;

            float dist = distance_3d(mg1->position, mg2->position);
            if (dist > diffusion_radius) continue;

            // Distance-weighted diffusion
            float weight = diffusion_rate * (1.0F - dist / diffusion_radius);

            for (int c = 0; c < NIMCP_CYTOKINE_COUNT; c++) {
                float c1 = mg1->cytokines.concentrations[c];
                float c2 = mg2->cytokines.concentrations[c];
                float diff = (c2 - c1) * weight;

                nimcp_spinlock_lock(&mg1->lock);
                mg1->cytokines.concentrations[c] += diff;
                nimcp_spinlock_unlock(&mg1->lock);

                nimcp_spinlock_lock(&mg2->lock);
                mg2->cytokines.concentrations[c] -= diff;
                nimcp_spinlock_unlock(&mg2->lock);
            }
        }
    }
}

void microglia_network_get_stats(const microglia_network_t* network,
                                  microglia_network_stats_t* stats)
{
    if (!stats) return;

    memset(stats, 0, sizeof(microglia_network_stats_t));

    if (!network) return;
    if (!network->microglia) return;
    /* Bounds validation: num_microglia should not exceed capacity */
    if (network->num_microglia > network->capacity) return;

    stats->total_microglia = network->num_microglia;

    float total_inflammation = 0.0F;
    float total_activity = 0.0F;
    uint32_t total_synapses = 0;

    for (uint32_t i = 0; i < network->num_microglia; i++) {
        microglia_t* mg = network->microglia[i];
        if (!mg) continue;

        stats->total_monitored_synapses += mg->num_monitored_synapses;
        stats->total_pruned += mg->total_synapses_pruned;
        stats->total_c1q_tagged += mg->total_c1q_tags;
        stats->total_c3_tagged += mg->total_c3_conversions;
        stats->total_protected += mg->protected_from_pruning;

        switch (mg->state) {
            case MICROGLIA_STATE_RAMIFIED: stats->ramified_count++; break;
            case MICROGLIA_STATE_ACTIVATED: stats->activated_count++; break;
            case MICROGLIA_STATE_PHAGOCYTIC: stats->phagocytic_count++; break;
        }

        total_inflammation += mg->state_variables[STATE_IDX_INFLAMMATION];

        for (uint32_t j = 0; j < mg->num_monitored_synapses; j++) {
            total_activity += mg->synapses[j].activity_score;
            total_synapses++;
        }

        // Sum cytokines
        stats->total_pro_inflammatory +=
            mg->cytokines.concentrations[CYTOKINE_IL1B] +
            mg->cytokines.concentrations[CYTOKINE_TNFA] +
            mg->cytokines.concentrations[CYTOKINE_IL6];

        stats->total_anti_inflammatory +=
            mg->cytokines.concentrations[CYTOKINE_IL10] +
            mg->cytokines.concentrations[CYTOKINE_TGFB];
    }

    if (network->num_microglia > 0) {
        stats->avg_inflammation = total_inflammation / network->num_microglia;
    }

    if (total_synapses > 0) {
        stats->avg_activity_score = total_activity / total_synapses;
    }
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

uint32_t microglia_get_total_pruned(microglia_t* mg)
{
    if (!mg) return 0;
    return mg->total_synapses_pruned;
}

const char* microglia_state_to_string(microglia_state_t state)
{
    switch (state) {
        case MICROGLIA_STATE_RAMIFIED: return "Ramified";
        case MICROGLIA_STATE_ACTIVATED: return "Activated";
        case MICROGLIA_STATE_PHAGOCYTIC: return "Phagocytic";
        default: return "Unknown";
    }
}

const char* cytokine_type_to_string(cytokine_type_t type)
{
    switch (type) {
        case CYTOKINE_IL1B: return "IL-1β";
        case CYTOKINE_TNFA: return "TNF-α";
        case CYTOKINE_IL6: return "IL-6";
        case CYTOKINE_IL10: return "IL-10";
        case CYTOKINE_TGFB: return "TGF-β";
        default: return "UNKNOWN";
    }
}

//=============================================================================
// MEMORY POOL FUNCTIONS (Phase 1.5+)
//=============================================================================

microglia_synapse_pool_t* microglia_synapse_pool_create(uint32_t capacity)
{
    if (capacity == 0) {
        capacity = NIMCP_MICROGLIA_SYNAPSE_POOL_SIZE;
    }

    microglia_synapse_pool_t* pool = (microglia_synapse_pool_t*)nimcp_malloc(
        sizeof(microglia_synapse_pool_t));
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;
    }

    memset(pool, 0, sizeof(microglia_synapse_pool_t));

    // Allocate synapse buffer
    pool->buffer = (monitored_synapse_t*)nimcp_malloc(
        capacity * sizeof(monitored_synapse_t));
    if (!pool->buffer) {
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "microglia_synapse_pool_create: pool->buffer is NULL");
        return NULL;
    }
    memset(pool->buffer, 0, capacity * sizeof(monitored_synapse_t));

    // Allocate bitmap (1 bit per slot, 64 slots per word)
    pool->num_bitmap_words = (capacity + 63) / 64;
    pool->bitmap = (uint64_t*)nimcp_malloc(pool->num_bitmap_words * sizeof(uint64_t));
    if (!pool->bitmap) {
        nimcp_free(pool->buffer);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "microglia_synapse_pool_create: pool->bitmap is NULL");
        return NULL;
    }

    // Initialize bitmap: all bits set = all slots free
    for (uint32_t i = 0; i < pool->num_bitmap_words; i++) {
        pool->bitmap[i] = UINT64_MAX;
    }

    // Clear bits beyond capacity (if capacity not multiple of 64)
    uint32_t extra_bits = pool->num_bitmap_words * 64 - capacity;
    if (extra_bits > 0) {
        pool->bitmap[pool->num_bitmap_words - 1] &= (UINT64_MAX >> extra_bits);
    }

    pool->capacity = capacity;
    pool->allocated_count = 0;
    nimcp_spinlock_init(&pool->lock);

    return pool;
}

void microglia_synapse_pool_destroy(microglia_synapse_pool_t* pool)
{
    if (!pool) return;

    if (pool->bitmap) {
        nimcp_free(pool->bitmap);
    }
    if (pool->buffer) {
        nimcp_free(pool->buffer);
    }
    nimcp_free(pool);
}

monitored_synapse_t* microglia_synapse_pool_alloc(microglia_synapse_pool_t* pool)
{
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&pool->lock);

    // Find first free slot using bitmap
    for (uint32_t word_idx = 0; word_idx < pool->num_bitmap_words; word_idx++) {
        if (pool->bitmap[word_idx] != 0) {
            // Find first set bit (free slot)
            uint64_t word = pool->bitmap[word_idx];
            int bit_idx = __builtin_ctzll(word);  // Count trailing zeros

            uint32_t slot_idx = word_idx * 64 + bit_idx;
            if (slot_idx >= pool->capacity) {
                break;  // Beyond capacity
            }

            // Clear bit (mark as allocated)
            pool->bitmap[word_idx] &= ~(1ULL << bit_idx);
            pool->allocated_count++;

            monitored_synapse_t* synapse = &pool->buffer[slot_idx];
            memset(synapse, 0, sizeof(monitored_synapse_t));

            nimcp_spinlock_unlock(&pool->lock);
            return synapse;
        }
    }

    nimcp_spinlock_unlock(&pool->lock);
    return NULL;  // Pool exhausted
}

void microglia_synapse_pool_free(microglia_synapse_pool_t* pool,
                                  monitored_synapse_t* synapse)
{
    if (!pool || !synapse) return;

    // Verify synapse is within pool
    if (synapse < pool->buffer ||
        synapse >= pool->buffer + pool->capacity) {
        return;  // Not from this pool
    }

    nimcp_spinlock_lock(&pool->lock);

    uint32_t slot_idx = (uint32_t)(synapse - pool->buffer);
    uint32_t word_idx = slot_idx / 64;
    uint32_t bit_idx = slot_idx % 64;

    // Set bit (mark as free)
    pool->bitmap[word_idx] |= (1ULL << bit_idx);
    pool->allocated_count--;

    nimcp_spinlock_unlock(&pool->lock);
}

void microglia_synapse_pool_stats(const microglia_synapse_pool_t* pool,
                                   uint32_t* allocated, uint32_t* capacity)
{
    if (!pool) {
        if (allocated) *allocated = 0;
        if (capacity) *capacity = 0;
        return;
    }

    if (allocated) *allocated = pool->allocated_count;
    if (capacity) *capacity = pool->capacity;
}

//=============================================================================
// COPY-ON-WRITE FUNCTIONS (Phase 1.5+)
//=============================================================================

microglia_t* microglia_cow_copy(microglia_t* mg)
{
    if (!mg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mg is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&mg->lock);

    // Create shallow copy
    microglia_t* copy = (microglia_t*)nimcp_malloc(sizeof(microglia_t));
    if (!copy) {
        nimcp_spinlock_unlock(&mg->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "microglia_cow_copy: copy is NULL");
        return NULL;
    }

    // Copy all fields
    memcpy(copy, mg, sizeof(microglia_t));

    // Share array pointers (shallow copy)
    // The arrays (synapses, monitored_synapse_ids, etc.) are shared

    // Set up CoW tracking
    mg->cow_ref_count++;
    copy->cow_ref_count = 1;
    copy->cow_modified = false;
    copy->cow_original = mg;

    // Initialize separate lock for copy
    nimcp_spinlock_init(&copy->lock);

    nimcp_spinlock_unlock(&mg->lock);
    return copy;
}

nimcp_result_t microglia_cow_prepare_write(microglia_t* mg)
{
    NIMCP_CHECK_THROW(mg, NIMCP_ERROR_INVALID_PARAM, "mg is NULL");

    nimcp_spinlock_lock(&mg->lock);

    // If already modified or not a copy, nothing to do
    if (mg->cow_modified || !mg->cow_original) {
        nimcp_spinlock_unlock(&mg->lock);
        return NIMCP_SUCCESS;
    }

    // Need to deep copy the shared arrays
    microglia_t* original = (microglia_t*)mg->cow_original;

    // Deep copy synapse array
    if (original->synapses && original->max_monitored_synapses > 0) {
        mg->synapses = (monitored_synapse_t*)nimcp_malloc(
            mg->max_monitored_synapses * sizeof(monitored_synapse_t));
        if (!mg->synapses) {
            nimcp_spinlock_unlock(&mg->lock);
            return NIMCP_ERROR_MEMORY;
        }
        memcpy(mg->synapses, original->synapses,
               mg->num_monitored_synapses * sizeof(monitored_synapse_t));
    }

    // Deep copy legacy arrays (G7: synapse_id widened to uint64)
    if (original->monitored_synapse_ids && mg->max_monitored_synapses > 0) {
        mg->monitored_synapse_ids = (uint64_t*)nimcp_malloc(
            mg->max_monitored_synapses * sizeof(uint64_t));
        if (mg->monitored_synapse_ids) {
            memcpy(mg->monitored_synapse_ids, original->monitored_synapse_ids,
                   mg->num_monitored_synapses * sizeof(uint64_t));
        }
    }

    if (original->synapse_activity_scores && mg->max_monitored_synapses > 0) {
        mg->synapse_activity_scores = (float*)nimcp_malloc(
            mg->max_monitored_synapses * sizeof(float));
        if (mg->synapse_activity_scores) {
            memcpy(mg->synapse_activity_scores, original->synapse_activity_scores,
                   mg->num_monitored_synapses * sizeof(float));
        }
    }

    if (original->last_activity_times && mg->max_monitored_synapses > 0) {
        mg->last_activity_times = (uint64_t*)nimcp_malloc(
            mg->max_monitored_synapses * sizeof(uint64_t));
        if (mg->last_activity_times) {
            memcpy(mg->last_activity_times, original->last_activity_times,
                   mg->num_monitored_synapses * sizeof(uint64_t));
        }
    }

    // Mark as modified and decrement original's ref count
    mg->cow_modified = true;

    nimcp_spinlock_lock(&original->lock);
    if (original->cow_ref_count > 0) {
        original->cow_ref_count--;
    }
    nimcp_spinlock_unlock(&original->lock);

    mg->cow_original = NULL;

    nimcp_spinlock_unlock(&mg->lock);
    return NIMCP_SUCCESS;
}

void microglia_cow_release(microglia_t* mg)
{
    if (!mg) return;

    nimcp_spinlock_lock(&mg->lock);

    if (mg->cow_original && !mg->cow_modified) {
        // This is a shallow copy - decrement original's ref count
        microglia_t* original = (microglia_t*)mg->cow_original;
        nimcp_spinlock_lock(&original->lock);
        if (original->cow_ref_count > 0) {
            original->cow_ref_count--;
        }
        nimcp_spinlock_unlock(&original->lock);

        // Don't free shared arrays
        mg->synapses = NULL;
        mg->monitored_synapse_ids = NULL;
        mg->synapse_activity_scores = NULL;
        mg->last_activity_times = NULL;
    }

    nimcp_spinlock_unlock(&mg->lock);

    // Free the copy structure itself
    // Note: If cow_modified is true, arrays were deep-copied and will be freed
    // by microglia_destroy when called
}

bool microglia_is_cow_copy(const microglia_t* mg)
{
    if (!mg) {
        return false;
    }
    return (mg->cow_original != NULL);
}
