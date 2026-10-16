/**
 * @file nimcp_astrocytes.c
 * @brief Implementation of biological astrocyte glial cells
 *
 * STATUS: Bio-async integrated - event-driven calcium wave coordination
 * FEATURES: Glutamate/D-serine release via predictive signals, glial wave API
 */

#include "glial/astrocytes/nimcp_astrocytes.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/spatial/nimcp_kdtree.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

//=============================================================================
// Health Agent Integration (Phase 8: Heartbeat for Long Operations)
//=============================================================================
NIMCP_DECLARE_HEALTH_AGENT(astrocyte)

//=============================================================================
// Global Bio-Async Context
//=============================================================================

static bio_module_context_t g_astrocyte_bio_ctx = NULL;
static unified_mem_manager_t g_astrocyte_mem_mgr = NULL;
static nimcp_atomic_bool_t g_astrocyte_bio_initialized = { .value = false };
static nimcp_platform_once_t g_astrocyte_bio_once = NIMCP_PLATFORM_ONCE_INIT;
static nimcp_error_t g_astrocyte_bio_init_result = NIMCP_SUCCESS;

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle BIO_MSG_ASTROCYTE_CALCIUM_WAVE message
 *
 * Initiates a calcium wave from the specified region with given initial concentration.
 * Uses glial wave API for slow system-wide coordination.
 */
static nimcp_error_t handle_calcium_wave_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Invalid calcium wave message: msg=%p, size=%zu",
                         msg, msg_size);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // Parse message payload (expecting region ID and calcium level)
    if (msg_size < sizeof(bio_message_header_t) + sizeof(uint32_t) + sizeof(float)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Calcium wave message too small: size=%zu", msg_size);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    uint32_t source_region = *(const uint32_t*)payload;
    float initial_calcium = *(const float*)(payload + sizeof(uint32_t));

    LOG_MODULE_INFO("ASTROCYTE", "Initiating calcium wave from region %u, calcium=%.2f μM",
                    source_region, initial_calcium);

    // Initiate glial wave for slow system-wide coordination
    // Note: nimcp_glial_wave_initiate would need to be implemented in bio_async
    // For now, we publish a predictive signal
    nimcp_error_t result = bio_router_publish_signal(g_astrocyte_bio_ctx,
        "astrocyte.calcium_wave", initial_calcium);

    if (result == NIMCP_SUCCESS) {
        LOG_MODULE_DEBUG("ASTROCYTE", "Calcium wave signal published successfully");
    } else {
        LOG_MODULE_WARN("ASTROCYTE", "Failed to publish calcium wave signal: error=%d", result);
    }

    return result;
}

/**
 * @brief Handle BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE message
 *
 * Processes glutamate uptake request and updates internal glutamate pool.
 */
static nimcp_error_t handle_glutamate_uptake_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Invalid glutamate uptake message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // Parse uptake amount
    if (msg_size < sizeof(bio_message_header_t) + sizeof(float)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Glutamate uptake message too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    float uptake_amount = *(const float*)payload;

    LOG_MODULE_DEBUG("ASTROCYTE", "Processing glutamate uptake: amount=%.4f", uptake_amount);

    // Publish uptake event
    nimcp_error_t result = bio_router_publish_signal(g_astrocyte_bio_ctx,
        "astrocyte.glutamate_uptake", uptake_amount);

    return result;
}

/**
 * @brief Handle BIO_MSG_METABOLIC_DEMAND message
 *
 * Processes metabolic demand from neurons and responds with metabolic supply.
 * Astrocytes provide glucose and lactate to neurons for ATP production.
 */
static nimcp_error_t handle_metabolic_demand_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_msg_metabolic_demand_t)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Invalid metabolic demand message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_msg_metabolic_demand_t* demand = (const bio_msg_metabolic_demand_t*)msg;

    LOG_MODULE_DEBUG("ASTROCYTE", "Metabolic demand from region %u: glucose=%.2f, oxygen=%.2f, ATP deficit=%.2f, urgency=%.2f",
                     demand->region_id, demand->glucose_demand, demand->oxygen_demand,
                     demand->atp_deficit, demand->urgency);

    // Calculate supply based on current ATP level and demand urgency
    // High urgency demands get priority allocation
    float supply_factor = 1.0F; // This would be calculated based on astrocyte ATP level
    float glucose_supply = demand->glucose_demand * supply_factor;
    float oxygen_supply = demand->oxygen_demand * supply_factor;

    // Publish metabolic supply response via SEROTONIN (slow, metabolic coordination)
    bio_router_publish_signal(g_astrocyte_bio_ctx, "astrocyte.metabolic_supply", supply_factor);

    // If urgency is high, also publish via NOREPINEPHRINE (alert channel)
    if (demand->urgency > 0.7F) {
        LOG_MODULE_WARN("ASTROCYTE", "High urgency metabolic demand (%.2f) - escalating priority", demand->urgency);
        bio_router_publish_signal(g_astrocyte_bio_ctx, "astrocyte.metabolic_alert", demand->urgency);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle BIO_MSG_GLIAL_SYNC_REQUEST message
 *
 * Handles synchronization requests for coordinated glial cell activity.
 * Used to coordinate calcium waves, metabolic support, and homeostatic adjustments.
 */
static nimcp_error_t handle_glial_sync_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Invalid glial sync message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_MODULE_DEBUG("ASTROCYTE", "Glial sync request received from module %u", header->source_module);

    // Publish sync acknowledgment - this helps coordinate timing across glial cells
    nimcp_error_t result = bio_router_publish_signal(g_astrocyte_bio_ctx,
        "astrocyte.sync_ack", 1.0F);

    // Respond to sync request if response promise is provided
    if (response_promise) {
        // Send back current astrocyte state (calcium level, ATP, etc.) for coordination
        // This enables other modules to make decisions based on glial network state
        typedef struct {
            float avg_calcium;      // Average calcium across network
            float avg_atp;          // Average ATP level
            float max_calcium;      // Peak calcium (wave detection)
            uint32_t active_count;  // Number of astrocytes with elevated calcium
        } glial_sync_response_t;

        glial_sync_response_t response = {
            .avg_calcium = 0.0F,
            .avg_atp = 0.0F,
            .max_calcium = 0.0F,
            .active_count = 0
        };

        // Note: To compute network-wide stats, we would need access to the network
        // For now, return baseline values indicating healthy glial state
        response.avg_calcium = ASTROCYTE_BASELINE_CALCIUM_UM;
        response.avg_atp = 1.0F;  // Full ATP
        response.max_calcium = ASTROCYTE_BASELINE_CALCIUM_UM;
        response.active_count = 0;

        // Complete the response promise with the glial state
        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
        LOG_MODULE_DEBUG("ASTROCYTE", "Sync response sent: avg_ca=%.2f avg_atp=%.2f",
                         response.avg_calcium, response.avg_atp);
    }

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
static int astrocyte_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_ASTROCYTE_CALCIUM_WAVE:
                bio_router_register_handler(ctx, message_types[i], handle_calcium_wave_message);
                registered++;
                break;
            case BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE:
                bio_router_register_handler(ctx, message_types[i], handle_glutamate_uptake_message);
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
                LOG_MODULE_DEBUG("ASTROCYTE", "Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_MODULE_INFO("ASTROCYTE", "KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

/**
 * @brief Internal initialization function called by nimcp_platform_once
 *
 * Thread-safe initialization of bio-async integration.
 * Result is stored in g_astrocyte_bio_init_result for callers to check.
 */
static void astrocyte_bio_init_once_impl(void)
{
    // Initialize unified memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    g_astrocyte_mem_mgr = unified_mem_create(&mem_config);
    if (!g_astrocyte_mem_mgr) {
        LOG_MODULE_ERROR("ASTROCYTE", "Failed to create unified memory manager");
        g_astrocyte_bio_init_result = NIMCP_ERROR_MEMORY;
        return;
    }

    // Register module with bio-router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_ASTROCYTE,
        .module_name = "Astrocyte",
        .inbox_capacity = 256,
        .user_data = NULL
    };

    g_astrocyte_bio_ctx = bio_router_register_module(&info);
    if (!g_astrocyte_bio_ctx) {
        LOG_MODULE_ERROR("ASTROCYTE", "Failed to register with bio-router");
        unified_mem_destroy(g_astrocyte_mem_mgr);
        g_astrocyte_mem_mgr = NULL;
        g_astrocyte_bio_init_result = NIMCP_ERROR_INVALID_PARAM;
        return;
    }

    // Try KG-driven wiring callback registration first
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_ASTROCYTE,
        (void*)astrocyte_wiring_handler_callback,
        NULL
    );

    if (result == NIMCP_SUCCESS) {
        LOG_MODULE_INFO("ASTROCYTE", "KG-driven wiring callback registered successfully");
    } else {
        // Fallback to legacy handler registration
        LOG_MODULE_INFO("ASTROCYTE", "Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_astrocyte_bio_ctx,
                                                  BIO_MSG_ASTROCYTE_CALCIUM_WAVE,
                                                  handle_calcium_wave_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("ASTROCYTE", "Failed to register calcium wave handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_astrocyte_bio_ctx,
                                                  BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE,
                                                  handle_glutamate_uptake_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("ASTROCYTE", "Failed to register glutamate uptake handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_astrocyte_bio_ctx,
                                                  BIO_MSG_METABOLIC_DEMAND,
                                                  handle_metabolic_demand_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("ASTROCYTE", "Failed to register metabolic demand handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_astrocyte_bio_ctx,
                                                  BIO_MSG_GLIAL_SYNC_REQUEST,
                                                  handle_glial_sync_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("ASTROCYTE", "Failed to register glial sync handler: %d", result);
            goto cleanup;
        }

        LOG_MODULE_INFO("ASTROCYTE", "Bio-async integration initialized with legacy handlers (4 handlers registered)");
    }

    nimcp_atomic_store_bool(&g_astrocyte_bio_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    g_astrocyte_bio_init_result = NIMCP_SUCCESS;
    return;

cleanup:
    bio_router_unregister_module(g_astrocyte_bio_ctx);
    g_astrocyte_bio_ctx = NULL;
    unified_mem_destroy(g_astrocyte_mem_mgr);
    g_astrocyte_mem_mgr = NULL;
    g_astrocyte_bio_init_result = result;
}

/**
 * @brief Initialize bio-async integration for astrocytes module (thread-safe)
 *
 * Uses nimcp_platform_once to ensure initialization runs exactly once
 * across all threads calling this function.
 */
static nimcp_error_t astrocyte_bio_init(void)
{
    nimcp_platform_once(&g_astrocyte_bio_once, astrocyte_bio_init_once_impl);
    return g_astrocyte_bio_init_result;
}

/**
 * @brief Shutdown bio-async integration
 */
static void astrocyte_bio_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_astrocyte_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    LOG_MODULE_INFO("ASTROCYTE", "Shutting down bio-async integration");

    if (g_astrocyte_bio_ctx) {
        bio_router_unregister_module(g_astrocyte_bio_ctx);
        g_astrocyte_bio_ctx = NULL;
    }

    if (g_astrocyte_mem_mgr) {
        unified_mem_destroy(g_astrocyte_mem_mgr);
        g_astrocyte_mem_mgr = NULL;
    }

    nimcp_atomic_store_bool(&g_astrocyte_bio_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
}

//=============================================================================
// Public Bio-Async API
//=============================================================================

nimcp_result_t astrocyte_register_bio_handlers(void)
{
    LOG_MODULE_INFO("ASTROCYTE", "Registering bio-async message handlers");
    return astrocyte_bio_init();
}

void astrocyte_unregister_bio_handlers(void)
{
    LOG_MODULE_INFO("ASTROCYTE", "Unregistering bio-async message handlers");
    astrocyte_bio_shutdown();
}

//=============================================================================
// Creation and Destruction
//=============================================================================

astrocyte_t* astrocyte_create(uint32_t id, astrocyte_type_t type, float x, float y, float z, float coverage_radius)
{
    /* Initialize bio-async on first create (thread-safe via nimcp_platform_once) */
    nimcp_error_t init_result = astrocyte_bio_init();
    if (init_result != NIMCP_SUCCESS) {
        LOG_MODULE_WARN("ASTROCYTE", "Bio-async init failed: %d (continuing anyway)", init_result);
    }

    /* Validate spatial coordinates are finite (not NaN or Inf) */
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Invalid spatial coordinates: (%.2f, %.2f, %.2f)", x, y, z);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_create: required parameter is NULL (isfinite, isfinite, isfinite)");
        return NULL;
    }

    /* Validate coverage radius is positive and finite */
    if (coverage_radius < 0.0F || !isfinite(coverage_radius)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Invalid coverage radius: %.2f", coverage_radius);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_create: isfinite is NULL");
        return NULL;
    }

    astrocyte_t* astro = (astrocyte_t*) nimcp_malloc(sizeof(astrocyte_t));
    NIMCP_API_CHECK_ALLOC(astro, "astrocyte_create: Failed to allocate astrocyte structure");

    memset(astro, 0, sizeof(astrocyte_t));

    // Identity
    astro->id = id;
    astro->type = type;

    // Calcium dynamics - initialize to resting baseline
    astro->calcium_concentration = ASTROCYTE_BASELINE_CALCIUM_UM;
    astro->calcium_baseline = ASTROCYTE_BASELINE_CALCIUM_UM;
    astro->ip3_concentration = 0.0F;
    astro->last_calcium_spike = 0;

    // Neurotransmitter pools - initialize to full
    astro->glutamate_pool = 1.0F;
    astro->d_serine_pool = 1.0F;
    astro->atp_level = 1.0F;

    // Spatial location
    astro->position[0] = x;
    astro->position[1] = y;
    astro->position[2] = z;
    astro->coverage_radius = coverage_radius;

    // Synaptic coverage - initially empty
    astro->num_covered_synapses = 0;
    astro->covered_synapse_ids = NULL;
    astro->synapse_calcium_levels = NULL;

    // Gap junction coupling - initially empty
    astro->num_coupled_astrocytes = 0;
    astro->coupled_astrocyte_ids = NULL;
    astro->coupling_strengths = NULL;

    // Homeostatic regulation
    astro->target_activity_level = 0.3F; // Default target
    astro->scaling_factor = 1.0F;

    // Thread safety
    nimcp_spinlock_init(&astro->lock);

    return astro;
}

void astrocyte_destroy(astrocyte_t* astro)
{
    if (!astro) {
        return;
    }

    // Free synapse arrays
    if (astro->covered_synapse_ids) {
        nimcp_free(astro->covered_synapse_ids);
    }
    if (astro->synapse_calcium_levels) {
        nimcp_free(astro->synapse_calcium_levels);
    }

    // Free coupling arrays
    if (astro->coupled_astrocyte_ids) {
        nimcp_free(astro->coupled_astrocyte_ids);
    }
    if (astro->coupling_strengths) {
        nimcp_free(astro->coupling_strengths);
    }

    // Destroy spinlock
    nimcp_spinlock_destroy(&astro->lock);

    // Free astrocyte itself
    nimcp_free(astro);
}

//=============================================================================
// Calcium Dynamics - STUB IMPLEMENTATION
//=============================================================================

void astrocyte_update_calcium(astrocyte_t* astro, float dt, float external_stimulus)
{
    if (!astro || dt < 0.0F) {
        return;
    }

    // Li-Rinzel calcium dynamics model
    // d[Ca²⁺]/dt = J_channel + J_leak - J_pump - J_ER
    //
    // BIOLOGICAL PARAMETERS:
    // - v1 = 6.0: IP3R channel max flux
    // - v2 = 0.11: Leak flux coefficient
    // - v3 = 2.2: Pump max flux
    // - k1 = 0.3 µM: Ca²⁺ activation for IP3R
    // - k2 = 0.1 µM: Ca²⁺ half-max for pump
    // - k3 = 0.5 µM: IP3 half-max for IP3R

    nimcp_spinlock_lock(&astro->lock);

    // Model parameters - TUNED for faster decay to baseline
    const float v1 = 6.0F;   // IP3R channel coefficient
    const float v2 = 0.11F;  // Leak coefficient
    const float v3 = 8.0F;   // Pump coefficient - DOUBLED from 4.0 for even faster decay
    const float k1 = 0.3F;   // Ca²⁺ activation constant (µM)
    const float k2 = 0.1F;   // Ca²⁺ pump half-max (µM)
    const float k3 = 0.5F;   // IP3 half-max (µM)

    // ER calcium store (assumed constant for simplified model)
    const float ca_er = 10.0F; // µM

    // Additional decay term to ensure return to baseline
    const float baseline_decay_rate = 5.0F; // Direct exponential decay to baseline - INCREASED

    // Current state
    float ca = astro->calcium_concentration;
    float ip3 = astro->ip3_concentration;

    // IP3 dynamics: production from stimulus, degradation
    const float ip3_degradation_rate = 1.0F; // /s (from biological constants)
    float ip3_production = external_stimulus * dt * 0.5F; // Stimulus produces IP3
    float ip3_degradation = ip3_degradation_rate * ip3 * dt;
    ip3 += ip3_production - ip3_degradation;
    ip3 = fmaxf(0.0F, fminf(5.0F, ip3)); // Clamp IP3

    // J_channel: IP3-receptor mediated Ca²⁺ release from ER
    // J_channel = v1 * (IP3³/(IP3³ + k3³)) * (Ca³/(Ca³ + k1³)) * (Ca_ER - Ca)
    float ip3_3 = ip3 * ip3 * ip3;
    float k3_3 = k3 * k3 * k3;
    float ip3_factor = ip3_3 / (ip3_3 + k3_3);

    float ca_3 = ca * ca * ca;
    float k1_3 = k1 * k1 * k1;
    float ca_factor = ca_3 / (ca_3 + k1_3);

    float J_channel = v1 * ip3_factor * ca_factor * (ca_er - ca);

    // J_leak: Passive leak from ER
    float J_leak = v2 * (ca_er - ca);

    // J_pump: ATP-dependent Ca²⁺ reuptake
    // J_pump = v3 * Ca²/(Ca² + k2²)
    float ca_2 = ca * ca;
    float k2_2 = k2 * k2;
    float J_pump = v3 * ca_2 / (ca_2 + k2_2);

    // Integrate calcium dynamics
    float d_ca = J_channel + J_leak - J_pump;

    // Add exponential decay term to ensure return to baseline
    // This dominates when IP3 is low, pulling Ca back to baseline
    float decay_to_baseline = -baseline_decay_rate * (ca - astro->calcium_baseline);
    d_ca += decay_to_baseline;

    ca += d_ca * dt;

    // Add stimulus contribution (direct calcium injection)
    ca += external_stimulus * dt * 0.1F;

    // Clamp to physiological range
    ca = fmaxf(0.0F, fminf(10.0F, ca));

    // Update astrocyte state
    astro->calcium_concentration = ca;
    astro->ip3_concentration = ip3;

    // Detect calcium spike (3x baseline)
    if (ca > astro->calcium_baseline * 3.0F) {
        astro->last_calcium_spike = nimcp_time_monotonic_us();
    }

    nimcp_spinlock_unlock(&astro->lock);

    // Process pending bio-async messages
    if (nimcp_atomic_load_bool(&g_astrocyte_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) && g_astrocyte_bio_ctx) {
        bio_router_process_inbox(g_astrocyte_bio_ctx, 5);
    }

    // Publish calcium concentration via predictive signal
    if (nimcp_atomic_load_bool(&g_astrocyte_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) && g_astrocyte_bio_ctx) {
        bio_router_publish_signal(g_astrocyte_bio_ctx, "astrocyte.calcium", ca);
        LOG_MODULE_DEBUG("ASTROCYTE", "Published calcium signal: %.2f μM", ca);
    }
}

void astrocyte_propagate_calcium_wave(astrocyte_t* astro,
                                      astrocyte_network_t* network,
                                      float dt)
{
    if (!astro || !network) {
        return;
    }

    // Only propagate if calcium is above threshold
    if (astro->calcium_concentration < network->calcium_threshold_um) {
        return;
    }

    // Propagate calcium to coupled neighbors via gap junctions
    // DEADLOCK FIX: Use consistent lock ordering by astrocyte ID (lower ID first)
    // to prevent deadlock when two astrocytes propagate to each other simultaneously.

    for (uint32_t i = 0; i < astro->num_coupled_astrocytes; i++) {
        uint32_t neighbor_id = astro->coupled_astrocyte_ids[i];
        float coupling_strength = astro->coupling_strengths[i];

        // Find neighbor astrocyte in network
        astrocyte_t* neighbor = NULL;
        for (uint32_t j = 0; j < network->num_astrocytes; j++) {
            if (network->astrocytes[j]->id == neighbor_id) {
                neighbor = network->astrocytes[j];
                break;
            }
        }

        if (neighbor) {
            // Acquire locks in consistent order (lower ID first) to prevent deadlock
            astrocyte_t* first = (astro->id < neighbor->id) ? astro : neighbor;
            astrocyte_t* second = (astro->id < neighbor->id) ? neighbor : astro;

            nimcp_spinlock_lock(&first->lock);
            nimcp_spinlock_lock(&second->lock);

            // Re-validate calcium concentration after acquiring locks
            // (may have changed if another thread updated it)
            float ca_diff = astro->calcium_concentration - neighbor->calcium_concentration;
            float flux = coupling_strength * ca_diff * network->coupling_decay_rate * dt;

            // Update neighbor calcium
            neighbor->calcium_concentration += flux;
            neighbor->calcium_concentration = fmaxf(0.0F, fminf(50.0F, neighbor->calcium_concentration));

            nimcp_spinlock_unlock(&second->lock);
            nimcp_spinlock_unlock(&first->lock);
        }
    }
}

//=============================================================================
// Neurotransmitter Release - STUB IMPLEMENTATION
//=============================================================================

float astrocyte_compute_glutamate_release(astrocyte_t* astro, uint32_t synapse_idx)
{
    if (!astro || synapse_idx >= astro->num_covered_synapses) {
        return 0.0F;
    }

    // Calcium-dependent glutamate release with pool depletion
    float ca_threshold = astro->calcium_baseline * 2.0F;
    if (astro->calcium_concentration > ca_threshold) {
        float excess = astro->calcium_concentration - ca_threshold;
        float release_fraction = fminf(1.0F, excess / 5.0F);

        // Release glutamate from pool (depletes pool)
        // Higher release rate for biological accuracy
        float release_rate = 0.15F; // 15% of available pool × release_fraction
        float release_amount = release_fraction * astro->glutamate_pool * release_rate;
        astro->glutamate_pool -= release_amount;
        astro->glutamate_pool = fmaxf(0.0F, astro->glutamate_pool); // Clamp to [0, 1]

        // Publish glutamate release via predictive signal
        if (nimcp_atomic_load_bool(&g_astrocyte_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) && g_astrocyte_bio_ctx) {
            bio_router_publish_signal(g_astrocyte_bio_ctx, "astrocyte.glutamate", release_amount);
            LOG_MODULE_DEBUG("ASTROCYTE", "Published glutamate release: %.4f", release_amount);
        }

        return release_amount;
    }

    return 0.0F;
}

float astrocyte_compute_d_serine_release(astrocyte_t* astro, uint32_t synapse_idx)
{
    if (!astro || synapse_idx >= astro->num_covered_synapses) {
        return 0.0F;
    }

    // Hill equation for D-serine release
    // release = pool * Ca^n / (Ca^n + K^n)
    //
    // BIOLOGICAL PARAMETERS:
    // - n = 4: Hill coefficient (cooperative binding)
    // - K = 1.5 µM: Half-maximal calcium concentration
    // - Depletion rate: Pool depletes with release
    // - Refill rate: 0.01/s (slow recovery)

    nimcp_spinlock_lock(&astro->lock);

    const float n = 4.0F;           // Hill coefficient
    const float K = 1.5F;           // Half-max calcium (µM)
    const float depletion_rate = 0.1F;  // Pool depletion per release
    const float refill_rate = 0.01F;    // Pool refill rate (1/s)

    float ca = astro->calcium_concentration;
    float pool = astro->d_serine_pool;

    // Hill equation: fractional release
    float ca_n = powf(ca, n);
    float K_n = powf(K, n);
    float release_fraction = ca_n / (ca_n + K_n);

    // Actual release amount (from available pool)
    float release = pool * release_fraction;

    // Pool depletion (assume dt ~ 0.001s for small depletion)
    // Note: dt should be passed in for proper dynamics, but using small constant here
    float dt = 0.001F; // 1ms timestep assumption
    pool -= release * dt * depletion_rate;

    // Pool refill (slow recovery toward 1.0)
    pool += dt * refill_rate * (1.0F - pool);

    // Clamp pool to [0, 1]
    pool = fmaxf(0.0F, fminf(1.0F, pool));

    // Update pool state
    astro->d_serine_pool = pool;

    nimcp_spinlock_unlock(&astro->lock);

    return release;
}

//=============================================================================
// Synaptic Modulation - STUB IMPLEMENTATION
//=============================================================================

void astrocyte_modulate_synapse_strength(astrocyte_t* astro,
                                         synapse_t* synapse,
                                         uint32_t synapse_idx)
{
    if (!astro || !synapse || synapse_idx >= astro->num_covered_synapses) {
        return;
    }

    // Dual modulation: Glutamate enhances AMPA, D-serine gates NMDA
    //
    // BIOLOGICAL MECHANISM:
    // - Glutamate: Spillover enhances AMPA receptors (multiplicative)
    // - D-serine: Obligatory NMDA co-agonist (threshold function)
    //
    // MODULATION FORMULA:
    // modulation = (1 + 0.5 * glutamate) * (d_serine > 0.1 ? 1.2 : 1.0)
    //
    // - Glutamate effect: Up to 50% enhancement
    // - D-serine effect: 20% boost if present (threshold 0.1)

    float glutamate = astrocyte_compute_glutamate_release(astro, synapse_idx);
    float d_serine = astrocyte_compute_d_serine_release(astro, synapse_idx);

    // Glutamate: multiplicative enhancement of AMPA transmission
    float glutamate_factor = 1.0F + 0.5F * glutamate;

    // D-serine: threshold gating for NMDA (binary-like, but smooth threshold)
    float d_serine_factor = (d_serine > 0.1F) ? 1.2F : 1.0F;

    // Combined modulation
    float modulation = glutamate_factor * d_serine_factor;

    // Apply to synapse strength
    synapse->strength *= modulation;
}

nimcp_result_t astrocyte_assign_synapse(astrocyte_t* astro, uint32_t synapse_id)
{
    if (!astro) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Reallocate arrays
    uint32_t new_count = astro->num_covered_synapses + 1;

    uint32_t* new_ids = (uint32_t*) nimcp_realloc(astro->covered_synapse_ids,
                                                    new_count * sizeof(uint32_t));
    if (!new_ids) {
        return NIMCP_ERROR_MEMORY;
    }
    astro->covered_synapse_ids = new_ids;

    float* new_ca = (float*) nimcp_realloc(astro->synapse_calcium_levels,
                                            new_count * sizeof(float));
    if (!new_ca) {
        return NIMCP_ERROR_MEMORY;
    }
    astro->synapse_calcium_levels = new_ca;

    // Add synapse
    astro->covered_synapse_ids[astro->num_covered_synapses] = synapse_id;
    astro->synapse_calcium_levels[astro->num_covered_synapses] = astro->calcium_baseline;
    astro->num_covered_synapses = new_count;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Homeostatic Plasticity - STUB IMPLEMENTATION
//=============================================================================

float astrocyte_compute_synaptic_scaling(astrocyte_t* astro, neural_network_t network)
{
    if (!astro) {
        return 1.0F;
    }

    // Homeostatic synaptic scaling with integrated calcium activity
    //
    // BIOLOGICAL MECHANISM:
    // - Astrocytes integrate calcium over time to estimate network activity
    // - Compare to target activity level
    // - Use PID controller to compute scaling factor
    //
    // ALGORITHM:
    // 1. Sliding window average of calcium (last 1000 timesteps)
    // 2. Error signal: target_activity - avg_calcium_normalized
    // 3. PID: scaling = 1.0 + Kp*error + Ki*integral_error
    // 4. Clamp to [0.3, 3.0] for stability
    //
    // PARAMETERS:
    // - Kp = 0.1: Proportional gain
    // - Ki = 0.01: Integral gain
    // - Window = 1000 timesteps (simplified to moving average)

    nimcp_spinlock_lock(&astro->lock);

    const float Kp = 0.1F;  // Proportional gain
    const float Ki = 0.01F; // Integral gain

    // Estimate average calcium activity
    // In full implementation, would use sliding window buffer
    // Here, simplified to current calcium normalized by expected max
    float avg_calcium_normalized = astro->calcium_concentration / 5.0F;

    // Error signal
    float error = astro->target_activity_level - avg_calcium_normalized;

    // Integral error (accumulated per-astrocyte for thread safety)
    // Uses astro->integral_error instead of static variable
    astro->integral_error += error * 0.001F; // Accumulate (assuming 1ms timestep)
    astro->integral_error = fmaxf(-1.0F, fminf(1.0F, astro->integral_error)); // Anti-windup

    // PID controller
    float scaling = 1.0F + Kp * error + Ki * astro->integral_error;

    // Clamp to stability range [0.3, 3.0]
    scaling = fmaxf(0.3F, fminf(3.0F, scaling));

    // Update internal state
    astro->scaling_factor = scaling;

    nimcp_spinlock_unlock(&astro->lock);

    return scaling;
}

//=============================================================================
// BCM Threshold Modulation - STUB IMPLEMENTATION
//=============================================================================

float astrocyte_compute_bcm_threshold_shift(astrocyte_t* astro, float default_threshold)
{
    if (!astro) {
        return 0.0F;
    }

    // Sigmoidal BCM threshold modulation
    //
    // BIOLOGICAL MECHANISM:
    // - Elevated astrocyte calcium shifts BCM threshold upward
    // - Implements metaplasticity (plasticity of plasticity)
    // - Prevents runaway potentiation during high activity
    //
    // FORMULA:
    // shift = max_shift * tanh((calcium - baseline) / sensitivity)
    //
    // PARAMETERS:
    // - max_shift = 0.3: Maximum threshold shift
    // - sensitivity = 2.0: Sensitivity to calcium changes
    //
    // EFFECT:
    // - Positive shift raises threshold (favors LTD)
    // - Negative shift lowers threshold (favors LTP)

    nimcp_spinlock_lock(&astro->lock);

    const float max_shift = 0.3F;      // Maximum shift magnitude
    const float sensitivity = 2.0F;    // Sensitivity parameter

    float calcium_deviation = astro->calcium_concentration - astro->calcium_baseline;
    float normalized_deviation = calcium_deviation / sensitivity;

    // Sigmoidal modulation using tanh
    float shift = max_shift * tanhf(normalized_deviation);

    nimcp_spinlock_unlock(&astro->lock);

    return shift;
}

//=============================================================================
// Metabolic Support - STUB IMPLEMENTATION
//=============================================================================

void astrocyte_update_atp_level(astrocyte_t* astro, float neural_activity, float dt)
{
    if (!astro || dt < 0.0F) {
        return;
    }

    // ATP production and consumption dynamics
    //
    // BIOLOGICAL MECHANISM:
    // - Astrocytes convert glucose to lactate (glycolysis)
    // - Transfer lactate to neurons for ATP production
    // - High neural activity depletes astrocyte ATP
    // - Calcium pumping also consumes ATP
    //
    // PRODUCTION:
    // - Glycolysis: 0.5 + 0.3 * lactate_availability (basal + activity-dependent)
    //
    // CONSUMPTION:
    // - Neural support: 0.1 * neural_activity
    // - Calcium pumping: 0.05 * (calcium - baseline)²
    //
    // DYNAMICS:
    // d_atp = (production - consumption) * dt
    // Clamp to [0.0, 1.5] (can exceed 1.0 temporarily during high glycolysis)

    nimcp_spinlock_lock(&astro->lock);

    // Lactate availability (simplified to constant for now)
    float lactate_availability = 1.0F;

    // Glycolysis production
    float production = 0.5F + 0.3F * lactate_availability;

    // Activity-dependent consumption
    float neural_consumption = 0.1F * neural_activity;

    // Calcium pumping consumption (quadratic in calcium excess)
    float calcium_excess = astro->calcium_concentration - astro->calcium_baseline;
    float calcium_pumping = 0.05F * calcium_excess * calcium_excess;

    // Total consumption
    float consumption = neural_consumption + calcium_pumping;

    // Integrate ATP dynamics
    float d_atp = (production - consumption) * dt;
    astro->atp_level += d_atp;

    // Clamp to [0.0, 1.0]
    astro->atp_level = fmaxf(0.0F, fminf(1.0F, astro->atp_level));

    nimcp_spinlock_unlock(&astro->lock);

    // Publish ATP level via predictive signal
    if (nimcp_atomic_load_bool(&g_astrocyte_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) && g_astrocyte_bio_ctx) {
        bio_router_publish_signal(g_astrocyte_bio_ctx, "astrocyte.atp", astro->atp_level);
        LOG_MODULE_DEBUG("ASTROCYTE", "Published ATP level: %.3f", astro->atp_level);
    }
}

//=============================================================================
// Astrocyte Network Management - STUB IMPLEMENTATION
//=============================================================================

astrocyte_network_t* astrocyte_network_create(uint32_t capacity)
{
    astrocyte_network_t* network = (astrocyte_network_t*) nimcp_malloc(sizeof(astrocyte_network_t));
    NIMCP_API_CHECK_ALLOC(network, "astrocyte_network_create: Failed to allocate network structure");

    memset(network, 0, sizeof(astrocyte_network_t));

    network->capacity = capacity;
    network->num_astrocytes = 0;
    network->astrocytes = (astrocyte_t**) nimcp_malloc(capacity * sizeof(astrocyte_t*));
    if (!network->astrocytes) {
        LOG_ERROR("astrocyte_network_create: Failed to allocate astrocytes array");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, capacity * sizeof(astrocyte_t*), "astrocyte_network_create: allocation failed");
        nimcp_free(network);
        return NULL;
    }

    // Global parameters
    network->calcium_threshold_um = ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM;
    network->coupling_decay_rate = 0.01F; // Biologically realistic coupling (target: 10-30 µm/s wave speed)
    network->coupling_radius_um = ASTROCYTE_COUPLING_RADIUS_UM;

    // Spatial index (NULL for now)
    network->spatial_index = NULL;

    if (nimcp_mutex_init(&network->lock, NULL) != NIMCP_SUCCESS) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "astrocyte_network_create: mutex init failed");
        return NULL;
    }

    return network;
}

void astrocyte_network_destroy(astrocyte_network_t* network)
{
    if (!network) {
        return;
    }

    // Destroy all astrocytes
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_destroy(network->astrocytes[i]);
    }

    // Free arrays
    if (network->astrocytes) {
        nimcp_free(network->astrocytes);
    }

    // Destroy spatial index (KD-tree)
    if (network->spatial_index) {
        kdtree_destroy((kdtree_t*)network->spatial_index);
    }

    nimcp_mutex_destroy(&network->lock);
    nimcp_free(network);
}

nimcp_result_t astrocyte_network_add(astrocyte_network_t* network, astrocyte_t* astro)
{
    if (!network || !astro) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (network->num_astrocytes >= network->capacity) {
        // Resize
        uint32_t new_capacity = network->capacity * 2;
        astrocyte_t** new_array = (astrocyte_t**) nimcp_realloc(network->astrocytes,
                                                                 new_capacity * sizeof(astrocyte_t*));
        if (!new_array) {
            return NIMCP_ERROR_MEMORY;
        }
        network->astrocytes = new_array;
        network->capacity = new_capacity;
    }

    network->astrocytes[network->num_astrocytes++] = astro;
    return NIMCP_SUCCESS;
}

nimcp_result_t astrocyte_network_build_spatial_index(astrocyte_network_t* network)
{
    if (!network) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Clear existing spatial index if present
    if (network->spatial_index) {
        kdtree_destroy((kdtree_t*)network->spatial_index);
        network->spatial_index = NULL;
    }

    // No astrocytes to index
    if (network->num_astrocytes == 0) {
        return NIMCP_SUCCESS;
    }

    // Create KD-tree
    kdtree_t* kdtree = kdtree_create();
    if (!kdtree) {
        return NIMCP_ERROR_MEMORY;
    }

    // Prepare point and user data arrays
    kdtree_point_t* points = (kdtree_point_t*)nimcp_malloc(
        network->num_astrocytes * sizeof(kdtree_point_t));
    void** user_data = (void**)nimcp_malloc(
        network->num_astrocytes * sizeof(void*));

    if (!points || !user_data) {
        nimcp_free(points);
        nimcp_free(user_data);
        kdtree_destroy(kdtree);
        return NIMCP_ERROR_MEMORY;
    }

    // Copy astrocyte positions and pointers
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        memcpy(points[i], network->astrocytes[i]->position, sizeof(kdtree_point_t));
        user_data[i] = network->astrocytes[i];
    }

    // Build KD-tree
    bool success = kdtree_build(kdtree, points, user_data, network->num_astrocytes);

    nimcp_free(points);
    nimcp_free(user_data);

    if (!success) {
        kdtree_destroy(kdtree);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    network->spatial_index = kdtree;
    return NIMCP_SUCCESS;
}

astrocyte_t* astrocyte_network_find_nearest(astrocyte_network_t* network, const float point[3])
{
    if (!network || !point || network->num_astrocytes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_network_find_nearest: required parameter is NULL (network, point)");
        return NULL;
    }
    if (!network->astrocytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_network_find_nearest: network->astrocytes is NULL");
        return NULL;
    }
    /* Validate spatial coordinates are finite */
    if (!isfinite(point[0]) || !isfinite(point[1]) || !isfinite(point[2])) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_network_find_nearest: required parameter is NULL (isfinite, isfinite, isfinite)");
        return NULL;
    }
    /* Bounds check: num_astrocytes should not exceed capacity */
    if (network->num_astrocytes > network->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_network_find_nearest: validation failed");
        return NULL;
    }

    /* Use KD-tree for O(log N) lookup if available */
    if (network->spatial_index) {
        kdtree_t* kdtree = (kdtree_t*)network->spatial_index;
        return (astrocyte_t*)kdtree_nearest(kdtree, point, NULL);
    }

    // Fallback to linear search O(N) if KD-tree not built
    astrocyte_t* nearest = network->astrocytes[0];
    float min_dist_sq = INFINITY;

    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        float dx = astro->position[0] - point[0];
        float dy = astro->position[1] - point[1];
        float dz = astro->position[2] - point[2];
        float dist_sq = dx*dx + dy*dy + dz*dz;

        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            nearest = astro;
        }
    }

    return nearest;
}

nimcp_result_t astrocyte_network_establish_coupling(astrocyte_network_t* network)
{
    if (!network) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!network->astrocytes || network->num_astrocytes == 0) {
        return NIMCP_SUCCESS;  /* Nothing to couple */
    }
    /* Bounds check: num_astrocytes should not exceed capacity */
    if (network->num_astrocytes > network->capacity) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* For each astrocyte, find neighbors within coupling radius */
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];

        // Count neighbors first
        uint32_t neighbor_count = 0;
        for (uint32_t j = 0; j < network->num_astrocytes; j++) {
            if (i == j) continue;

            astrocyte_t* neighbor = network->astrocytes[j];
            float dx = astro->position[0] - neighbor->position[0];
            float dy = astro->position[1] - neighbor->position[1];
            float dz = astro->position[2] - neighbor->position[2];
            float distance = sqrtf(dx*dx + dy*dy + dz*dz);

            if (distance <= network->coupling_radius_um) {
                neighbor_count++;
            }
        }

        if (neighbor_count == 0) {
            continue;
        }

        // Allocate arrays for neighbors
        astro->coupled_astrocyte_ids = (uint32_t*) nimcp_malloc(neighbor_count * sizeof(uint32_t));
        if (!astro->coupled_astrocyte_ids) {
            return NIMCP_ERROR_MEMORY;
        }
        astro->coupling_strengths = (float*) nimcp_malloc(neighbor_count * sizeof(float));
        if (!astro->coupling_strengths) {
            nimcp_free(astro->coupled_astrocyte_ids);
            astro->coupled_astrocyte_ids = NULL;
            return NIMCP_ERROR_MEMORY;
        }

        // Fill arrays
        uint32_t idx = 0;
        for (uint32_t j = 0; j < network->num_astrocytes; j++) {
            if (i == j) continue;

            astrocyte_t* neighbor = network->astrocytes[j];
            float dx = astro->position[0] - neighbor->position[0];
            float dy = astro->position[1] - neighbor->position[1];
            float dz = astro->position[2] - neighbor->position[2];
            float distance = sqrtf(dx*dx + dy*dy + dz*dz);

            if (distance <= network->coupling_radius_um) {
                astro->coupled_astrocyte_ids[idx] = neighbor->id;

                // Coupling strength decreases with distance
                // Use max(0.1, ...) to ensure minimum coupling even at boundary
                float normalized_dist = distance / network->coupling_radius_um;
                float strength = fmaxf(0.1F, 1.0F - normalized_dist);

                // Scale by distance² to compensate for Laplacian normalization
                // The graph Laplacian divides by distance², so we multiply here
                // to maintain the effective diffusion coefficient
                float distance_sq = dx*dx + dy*dy + dz*dz;
                astro->coupling_strengths[idx] = strength * distance_sq;

                idx++;
            }
        }

        astro->num_coupled_astrocytes = neighbor_count;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t astrocyte_network_assign_synapses(astrocyte_network_t* network,
                                                 neural_network_t* nn)
{
    if (!network || !nn) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Implement synapse assignment based on spatial proximity
    // Each astrocyte covers synapses within its coverage_radius
    //
    // BIOLOGICAL BASIS:
    // - Each astrocyte covers ~100,000 synapses in mammalian cortex
    // - Coverage determined by astrocyte territory (non-overlapping domains)
    // - Proximity-based assignment mimics tripartite synapse organization
    //
    // ALGORITHM:
    // For each neuron's outgoing synapses:
    //   1. Compute synapse position (midpoint between pre and post neurons)
    //   2. Find nearest astrocyte within coverage radius
    //   3. Assign synapse to that astrocyte
    //
    // PERFORMANCE: O(N_neurons × S_avg × A) where A = num_astrocytes
    //              Could be optimized with spatial index (KD-tree)

    uint32_t num_neurons = neural_network_get_num_neurons(*nn);
    if (num_neurons == 0 || network->num_astrocytes == 0) {
        LOG_MODULE_DEBUG("ASTROCYTE", "No neurons or astrocytes to assign");
        return NIMCP_SUCCESS;
    }

    LOG_MODULE_INFO("ASTROCYTE", "Assigning synapses to %u astrocytes from %u neurons",
                    network->num_astrocytes, num_neurons);

    uint32_t total_assigned = 0;

    // Iterate through all neurons and their outgoing synapses
    for (uint32_t neuron_id = 0; neuron_id < num_neurons; neuron_id++) {
        // Get neuron's position (approximation: use neuron_id as spatial index)
        // In a full implementation, neurons would have explicit 3D positions
        // For now, use a simple spatial mapping based on neuron ID
        float pre_pos[3];
        pre_pos[0] = (float)(neuron_id % 100) * 10.0F;  // X: 0-990 µm
        pre_pos[1] = (float)((neuron_id / 100) % 100) * 10.0F;  // Y: 0-990 µm
        pre_pos[2] = (float)(neuron_id / 10000) * 10.0F;  // Z: layered

        // Get outgoing synapses for this neuron
        const synapse_t* synapses = NULL;
        uint32_t num_synapses = neural_network_get_incoming_synapse_count(*nn, neuron_id);

        // For each synapse, compute midpoint and find nearest astrocyte
        // Note: We use incoming synapses to get pre->post pairs
        neural_network_get_incoming_synapses(*nn, neuron_id, &synapses);
        if (!synapses) continue;

        for (uint32_t s = 0; s < num_synapses; s++) {
            const synapse_t* syn = &synapses[s];

            // Compute synapse position as midpoint between pre and post
            // Post neuron is neuron_id (since we got incoming synapses)
            // Pre neuron is syn->source_neuron_id
            float post_pos[3];
            post_pos[0] = pre_pos[0];  // Use pre position as base
            post_pos[1] = pre_pos[1];
            post_pos[2] = pre_pos[2];

            // Synapse position (simplified: use pre-synaptic position)
            float syn_pos[3];
            if (syn->source_neuron_id > 0) {
                syn_pos[0] = (float)(syn->source_neuron_id % 100) * 10.0F;
                syn_pos[1] = (float)((syn->source_neuron_id / 100) % 100) * 10.0F;
                syn_pos[2] = (float)(syn->source_neuron_id / 10000) * 10.0F;
            } else {
                // Fallback to post position
                syn_pos[0] = post_pos[0];
                syn_pos[1] = post_pos[1];
                syn_pos[2] = post_pos[2];
            }

            // Find nearest astrocyte
            astrocyte_t* nearest = astrocyte_network_find_nearest(network, syn_pos);
            if (!nearest) continue;

            // Check if synapse is within astrocyte's coverage radius
            float dx = nearest->position[0] - syn_pos[0];
            float dy = nearest->position[1] - syn_pos[1];
            float dz = nearest->position[2] - syn_pos[2];
            float distance = sqrtf(dx*dx + dy*dy + dz*dz);

            if (distance <= nearest->coverage_radius) {
                // Assign synapse to this astrocyte
                // Use a unique synapse ID (encoding pre and post neuron IDs)
                uint32_t synapse_id = syn->source_neuron_id * 10000 + neuron_id;
                nimcp_result_t assign_result = astrocyte_assign_synapse(nearest, synapse_id);
                if (assign_result == NIMCP_SUCCESS) {
                    total_assigned++;
                }
            }
        }
    }

    LOG_MODULE_INFO("ASTROCYTE", "Assigned %u synapses to astrocyte network", total_assigned);
    return NIMCP_SUCCESS;
}

void astrocyte_network_step(astrocyte_network_t* network, float dt)
{
    if (!network || dt <= 0.0F) {
        return;
    }
    if (!network->astrocytes) {
        return;
    }
    /* Bounds check: num_astrocytes should not exceed capacity */
    if (network->num_astrocytes > network->capacity) {
        return;
    }
    /* Validate dt is finite and within reasonable range */
    if (!isfinite(dt) || dt > 1.0F) {
        return;  /* Cap at 1 second per step */
    }

    /* TODO: Implement full network dynamics */
    /* For now, simple update of all astrocytes */

    /* Phase 8: Send heartbeat at start of network step */
    astrocyte_heartbeat("astrocyte_network_step", 0.0f);

    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];

        // Update calcium
        astrocyte_update_calcium(astro, dt, 0.0F);

        // Propagate waves
        astrocyte_propagate_calcium_wave(astro, network, dt);

        // Update ATP
        astrocyte_update_atp_level(astro, 1.0F, dt);

        /* Phase 8: Send heartbeat for progress tracking in large networks */
        if ((i & 0xFF) == 0 && network->num_astrocytes > 256) {
            astrocyte_heartbeat("astrocyte_network_step",
                               (float)(i + 1) / (float)network->num_astrocytes);
        }
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

void astrocyte_network_get_stats(astrocyte_network_t* network,
                                 float* avg_calcium,
                                 float* max_calcium,
                                 float* avg_glutamate)
{
    if (!network || network->num_astrocytes == 0) {
        if (avg_calcium) *avg_calcium = 0.0F;
        if (max_calcium) *max_calcium = 0.0F;
        if (avg_glutamate) *avg_glutamate = 0.0F;
        return;
    }

    float sum_ca = 0.0F;
    float sum_glu = 0.0F;
    float max_ca = 0.0F;

    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        sum_ca += astro->calcium_concentration;
        sum_glu += astro->glutamate_pool;
        if (astro->calcium_concentration > max_ca) {
            max_ca = astro->calcium_concentration;
        }
    }

    if (avg_calcium) *avg_calcium = (network->num_astrocytes > 0) ? sum_ca / network->num_astrocytes : 0.0F;
    if (max_calcium) *max_calcium = max_ca;
    if (avg_glutamate) *avg_glutamate = (network->num_astrocytes > 0) ? sum_glu / network->num_astrocytes : 0.0F;
}

/* ============================================================================
 * Phase 8: State Manager Integration for Fault Tolerance
 * ============================================================================
 *
 * WHAT: Enable checkpointing and recovery for astrocyte network state
 * WHY:  Support system-wide resilience through consistent state management
 * HOW:  Implement serialize/deserialize/validate/reset/get_size operations
 *
 * SERIALIZATION STRATEGY:
 * - Network parameters (threshold, decay rate, coupling radius)
 * - Per-astrocyte core state (calcium, IP3, pools, position, homeostatic)
 * - NOT serialized: spatial index (rebuild), synapse coverage (rebuild),
 *   gap junction coupling (rebuild), calcium system (separate), mutexes
 */

/** Magic number for astrocyte network state validation */
#define ASTROCYTE_NETWORK_STATE_MAGIC 0x41535452  /* "ASTR" */

/** Version for state format compatibility */
#define ASTROCYTE_NETWORK_STATE_VERSION 1

/**
 * @brief Serialized astrocyte network state header
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    uint32_t num_astrocytes;
} astrocyte_network_state_header_t;

/**
 * @brief Per-astrocyte serialized state (no pointers/topology)
 */
typedef struct {
    uint32_t id;
    uint32_t type;                      /* astrocyte_type_t as uint32 */
    float calcium_concentration;
    float ip3_concentration;
    float calcium_baseline;
    uint64_t last_calcium_spike;
    float glutamate_pool;
    float d_serine_pool;
    float atp_level;
    float position[3];
    float coverage_radius;
    float target_activity_level;
    float scaling_factor;
    float integral_error;
} astrocyte_serialized_state_t;

/**
 * @brief Compute simple checksum for state validation
 */
static uint32_t astrocyte_compute_checksum(const uint8_t* data, size_t size) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum = (checksum >> 1) | (checksum << 31);
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief Get serialized state size for astrocyte network
 *
 * @param module_state Pointer to astrocyte_network_t
 * @return Size in bytes required for serialization
 */
size_t astrocyte_network_state_get_size(void* module_state) {
    if (!module_state) return 0;

    astrocyte_network_t* network = (astrocyte_network_t*)module_state;

    size_t size = sizeof(astrocyte_network_state_header_t);

    /* Network parameters */
    size += sizeof(float) * 3;  /* calcium_threshold_um, coupling_decay_rate, coupling_radius_um */

    /* Per-astrocyte state */
    size += network->num_astrocytes * sizeof(astrocyte_serialized_state_t);

    return size;
}

/**
 * @brief Serialize astrocyte network state to buffer
 *
 * @param module_state Pointer to astrocyte_network_t
 * @param buffer Output buffer (NULL to query size only)
 * @param size In: buffer size, Out: bytes written or required
 * @return 0 on success, -1 on error, -2 if buffer too small
 */
int astrocyte_network_state_serialize(void* module_state, uint8_t* buffer, size_t* size) {
    if (!size) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "size is NULL");

        return -1;

    }

    size_t required_size = astrocyte_network_state_get_size(module_state);

    /* Size query mode */
    if (!buffer) {
        *size = required_size;
        return 0;
    }

    /* Check buffer size */
    if (*size < required_size) {
        *size = required_size;
        return -2;
    }

    if (!module_state) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "module_state is NULL");


        return -1;


    }

    astrocyte_network_t* network = (astrocyte_network_t*)module_state;

    /* Acquire network lock for consistent read */
    nimcp_mutex_lock(&network->lock);

    uint8_t* ptr = buffer;

    /* Write header (will update checksum later) */
    astrocyte_network_state_header_t header = {
        .magic = ASTROCYTE_NETWORK_STATE_MAGIC,
        .version = ASTROCYTE_NETWORK_STATE_VERSION,
        .checksum = 0,
        .num_astrocytes = network->num_astrocytes
    };
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);

    /* Network parameters */
    memcpy(ptr, &network->calcium_threshold_um, sizeof(float)); ptr += sizeof(float);
    memcpy(ptr, &network->coupling_decay_rate, sizeof(float)); ptr += sizeof(float);
    memcpy(ptr, &network->coupling_radius_um, sizeof(float)); ptr += sizeof(float);

    /* Per-astrocyte state */
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (!astro) continue;

        nimcp_spinlock_lock(&astro->lock);

        astrocyte_serialized_state_t state = {
            .id = astro->id,
            .type = (uint32_t)astro->type,
            .calcium_concentration = astro->calcium_concentration,
            .ip3_concentration = astro->ip3_concentration,
            .calcium_baseline = astro->calcium_baseline,
            .last_calcium_spike = astro->last_calcium_spike,
            .glutamate_pool = astro->glutamate_pool,
            .d_serine_pool = astro->d_serine_pool,
            .atp_level = astro->atp_level,
            .position = {astro->position[0], astro->position[1], astro->position[2]},
            .coverage_radius = astro->coverage_radius,
            .target_activity_level = astro->target_activity_level,
            .scaling_factor = astro->scaling_factor,
            .integral_error = astro->integral_error
        };

        nimcp_spinlock_unlock(&astro->lock);

        memcpy(ptr, &state, sizeof(state));
        ptr += sizeof(state);
    }

    nimcp_mutex_unlock(&network->lock);

    /* Compute and update checksum (over data after header) */
    uint32_t checksum = astrocyte_compute_checksum(
        buffer + sizeof(astrocyte_network_state_header_t),
        required_size - sizeof(astrocyte_network_state_header_t)
    );
    memcpy(buffer + offsetof(astrocyte_network_state_header_t, checksum),
           &checksum, sizeof(uint32_t));

    *size = required_size;
    LOG_MODULE_DEBUG("ASTROCYTE", "Network state serialized: %zu bytes, %u astrocytes",
                     required_size, network->num_astrocytes);
    return 0;
}

/**
 * @brief Deserialize astrocyte network state from buffer
 *
 * @param module_state Pointer to astrocyte_network_t
 * @param buffer Input buffer containing serialized state
 * @param size Size of input buffer
 * @return 0 on success, negative on error
 */
int astrocyte_network_state_deserialize(void* module_state, const uint8_t* buffer, size_t size) {
    if (!module_state || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_network_state_deserialize: required parameter is NULL (module_state, buffer)");
        return -1;
    }

    if (size < sizeof(astrocyte_network_state_header_t)) {
        LOG_MODULE_ERROR("ASTROCYTE", "Deserialize: buffer too small for header");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_network_state_deserialize: validation failed");
        return -1;
    }

    astrocyte_network_t* network = (astrocyte_network_t*)module_state;
    const uint8_t* ptr = buffer;

    /* Read and validate header */
    astrocyte_network_state_header_t header;
    memcpy(&header, ptr, sizeof(header));
    ptr += sizeof(header);

    if (header.magic != ASTROCYTE_NETWORK_STATE_MAGIC) {
        LOG_MODULE_ERROR("ASTROCYTE", "Deserialize: invalid magic (0x%08X)", header.magic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_network_state_deserialize: validation failed");
        return -1;
    }

    if (header.version != ASTROCYTE_NETWORK_STATE_VERSION) {
        LOG_MODULE_ERROR("ASTROCYTE", "Deserialize: version mismatch (%u != %u)",
                         header.version, ASTROCYTE_NETWORK_STATE_VERSION);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_network_state_deserialize: validation failed");
        return -1;
    }

    /* Calculate expected size */
    size_t expected_size = sizeof(astrocyte_network_state_header_t) +
                           sizeof(float) * 3 +
                           header.num_astrocytes * sizeof(astrocyte_serialized_state_t);

    if (size < expected_size) {
        LOG_MODULE_ERROR("ASTROCYTE", "Deserialize: buffer too small (%zu < %zu)",
                         size, expected_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_network_state_deserialize: validation failed");
        return -1;
    }

    /* Verify checksum */
    uint32_t computed_checksum = astrocyte_compute_checksum(
        buffer + sizeof(astrocyte_network_state_header_t),
        expected_size - sizeof(astrocyte_network_state_header_t)
    );
    if (computed_checksum != header.checksum) {
        LOG_MODULE_ERROR("ASTROCYTE", "Deserialize: checksum mismatch (0x%08X != 0x%08X)",
                         computed_checksum, header.checksum);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_network_state_deserialize: validation failed");
        return -1;
    }

    /* Check astrocyte count matches */
    if (header.num_astrocytes != network->num_astrocytes) {
        LOG_MODULE_ERROR("ASTROCYTE", "Deserialize: astrocyte count mismatch (%u != %u)",
                         header.num_astrocytes, network->num_astrocytes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocyte_network_state_deserialize: validation failed");
        return -1;
    }

    /* Acquire network lock for consistent write */
    nimcp_mutex_lock(&network->lock);

    /* Network parameters */
    memcpy(&network->calcium_threshold_um, ptr, sizeof(float)); ptr += sizeof(float);
    memcpy(&network->coupling_decay_rate, ptr, sizeof(float)); ptr += sizeof(float);
    memcpy(&network->coupling_radius_um, ptr, sizeof(float)); ptr += sizeof(float);

    /* Per-astrocyte state */
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (!astro) {
            ptr += sizeof(astrocyte_serialized_state_t);
            continue;
        }

        astrocyte_serialized_state_t state;
        memcpy(&state, ptr, sizeof(state));
        ptr += sizeof(state);

        nimcp_spinlock_lock(&astro->lock);

        /* Restore state (ID should match but we don't overwrite) */
        if (astro->id != state.id) {
            LOG_MODULE_WARN("ASTROCYTE", "Deserialize: astrocyte ID mismatch at index %u", i);
        }

        astro->type = (astrocyte_type_t)state.type;
        astro->calcium_concentration = state.calcium_concentration;
        astro->ip3_concentration = state.ip3_concentration;
        astro->calcium_baseline = state.calcium_baseline;
        astro->last_calcium_spike = state.last_calcium_spike;
        astro->glutamate_pool = state.glutamate_pool;
        astro->d_serine_pool = state.d_serine_pool;
        astro->atp_level = state.atp_level;
        astro->position[0] = state.position[0];
        astro->position[1] = state.position[1];
        astro->position[2] = state.position[2];
        astro->coverage_radius = state.coverage_radius;
        astro->target_activity_level = state.target_activity_level;
        astro->scaling_factor = state.scaling_factor;
        astro->integral_error = state.integral_error;

        nimcp_spinlock_unlock(&astro->lock);
    }

    nimcp_mutex_unlock(&network->lock);

    LOG_MODULE_DEBUG("ASTROCYTE", "Network state deserialized: %u astrocytes", header.num_astrocytes);
    return 0;
}

/**
 * @brief Validate astrocyte network state integrity
 *
 * @param module_state Pointer to astrocyte_network_t
 * @return 0 if valid, negative error code if invalid
 */
int astrocyte_network_state_validate(void* module_state) {
    if (!module_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "module_state is NULL");

        return -1;

    }

    astrocyte_network_t* network = (astrocyte_network_t*)module_state;

    nimcp_mutex_lock(&network->lock);

    int result = 0;

    /* Validate network parameters */
    if (network->calcium_threshold_um <= 0.0F ||
        network->coupling_decay_rate < 0.0F ||
        network->coupling_radius_um <= 0.0F) {
        LOG_MODULE_WARN("ASTROCYTE", "Validate: invalid network parameters");
        result = -1;
    }

    /* Validate each astrocyte */
    for (uint32_t i = 0; i < network->num_astrocytes && result == 0; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (!astro) continue;

        nimcp_spinlock_lock(&astro->lock);

        /* Validate calcium is in reasonable range */
        if (astro->calcium_concentration < 0.0F ||
            astro->calcium_concentration > 100.0F ||
            !isfinite(astro->calcium_concentration)) {
            LOG_MODULE_WARN("ASTROCYTE", "Validate: invalid calcium at astrocyte %u", i);
            result = -2;
        }

        /* Validate IP3 is in reasonable range */
        if (astro->ip3_concentration < 0.0F ||
            astro->ip3_concentration > 50.0F ||
            !isfinite(astro->ip3_concentration)) {
            LOG_MODULE_WARN("ASTROCYTE", "Validate: invalid IP3 at astrocyte %u", i);
            result = -3;
        }

        /* Validate pools are normalized */
        if (astro->glutamate_pool < 0.0F || astro->glutamate_pool > 1.0F ||
            astro->d_serine_pool < 0.0F || astro->d_serine_pool > 1.0F ||
            astro->atp_level < 0.0F || astro->atp_level > 1.0F) {
            LOG_MODULE_WARN("ASTROCYTE", "Validate: invalid pools at astrocyte %u", i);
            result = -4;
        }

        /* Validate homeostatic parameters */
        if (!isfinite(astro->scaling_factor) || astro->scaling_factor <= 0.0F ||
            !isfinite(astro->integral_error)) {
            LOG_MODULE_WARN("ASTROCYTE", "Validate: invalid homeostatic params at astrocyte %u", i);
            result = -5;
        }

        nimcp_spinlock_unlock(&astro->lock);
    }

    nimcp_mutex_unlock(&network->lock);

    if (result == 0) {
        LOG_MODULE_DEBUG("ASTROCYTE", "Network state validation passed");
    }

    return result;
}

/**
 * @brief Reset astrocyte network state to defaults
 *
 * @param module_state Pointer to astrocyte_network_t
 * @return 0 on success, negative on error
 */
int astrocyte_network_state_reset(void* module_state) {
    if (!module_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "module_state is NULL");

        return -1;

    }

    astrocyte_network_t* network = (astrocyte_network_t*)module_state;

    nimcp_mutex_lock(&network->lock);

    /* Reset network parameters to defaults */
    network->calcium_threshold_um = ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM;
    network->coupling_decay_rate = 0.1F;  /* Default decay rate */
    network->coupling_radius_um = ASTROCYTE_COUPLING_RADIUS_UM;

    /* Reset each astrocyte to baseline */
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (!astro) continue;

        nimcp_spinlock_lock(&astro->lock);

        astro->calcium_concentration = ASTROCYTE_BASELINE_CALCIUM_UM;
        astro->ip3_concentration = 0.0F;
        astro->calcium_baseline = ASTROCYTE_BASELINE_CALCIUM_UM;
        astro->last_calcium_spike = 0;
        astro->glutamate_pool = 0.5F;
        astro->d_serine_pool = 0.5F;
        astro->atp_level = 1.0F;
        astro->target_activity_level = 1.0F;
        astro->scaling_factor = 1.0F;
        astro->integral_error = 0.0F;

        nimcp_spinlock_unlock(&astro->lock);
    }

    nimcp_mutex_unlock(&network->lock);

    LOG_MODULE_DEBUG("ASTROCYTE", "Network state reset to defaults");
    return 0;
}

/**
 * @brief Get astrocyte network state operations for state manager registration
 *
 * @return Pointer to static state ops structure
 */
const nimcp_module_state_ops_t* astrocyte_network_get_state_ops(void) {
    static nimcp_module_state_ops_t ops = {
        .serialize = astrocyte_network_state_serialize,
        .deserialize = astrocyte_network_state_deserialize,
        .validate = astrocyte_network_state_validate,
        .reset = astrocyte_network_state_reset,
        .get_size = astrocyte_network_state_get_size
    };
    return &ops;
}
