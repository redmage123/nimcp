/**
 * @file nimcp_oligodendrocytes.c
 * @brief Enhanced Oligodendrocyte Implementation with Bio-Async Integration
 *
 * IMPLEMENTATION FEATURES:
 * - Bio-async messaging for myelination via SEROTONIN channel (slow, stabilizing)
 * - RK4 ODE integration for myelination dynamics
 * - G-ratio optimization using Rushton's law
 * - Saltatory conduction velocity calculation
 * - NRG1/BDNF growth factor signaling
 * - Predictive signal publishing for myelination progress
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.1.0 - Bio-Async Integrated
 */

#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
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
#include "utils/time/nimcp_time.h"
#include "utils/spatial/nimcp_kdtree.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/platform/nimcp_platform_once.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(oligodendrocytes)

//=============================================================================
// Global Bio-Async Context
//=============================================================================

static bio_module_context_t g_oligo_bio_ctx = NULL;
static unified_mem_manager_t g_oligo_mem_mgr = NULL;
static nimcp_platform_once_t g_oligo_bio_once = NIMCP_PLATFORM_ONCE_INIT;
static nimcp_error_t g_oligo_bio_init_result = NIMCP_SUCCESS;

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle BIO_MSG_OLIGODENDROCYTE_MYELINATE message
 *
 * Processes myelination requests via SEROTONIN channel (slow, stabilizing).
 * Publishes myelination progress as predictive signals.
 */
static nimcp_error_t handle_myelination_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Invalid myelination message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // Parse myelination request (axon ID, target thickness, priority)
    if (msg_size < sizeof(bio_message_header_t) + sizeof(uint32_t) + 2 * sizeof(float)) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Myelination message too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    uint32_t axon_id = *(const uint32_t*)payload;
    float target_thickness = *(const float*)(payload + sizeof(uint32_t));
    float priority = *(const float*)(payload + sizeof(uint32_t) + sizeof(float));

    LOG_MODULE_INFO("OLIGODENDROCYTE", "Myelination request: axon=%u, thickness=%.2f μm, priority=%.2f",
                    axon_id, target_thickness, priority);

    // Publish myelination request via SEROTONIN (slow, stabilizing) channel
    nimcp_error_t result = bio_router_publish_signal(g_oligo_bio_ctx,
        "oligodendrocyte.myelination_request", priority);

    return result;
}

/**
 * @brief Handle BIO_MSG_METABOLIC_DEMAND message
 *
 * Oligodendrocytes are metabolically expensive - myelination requires significant ATP.
 * High metabolic demand may slow or pause myelination to preserve energy.
 */
static nimcp_error_t handle_metabolic_demand_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_msg_metabolic_demand_t)) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Invalid metabolic demand message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_msg_metabolic_demand_t* demand = (const bio_msg_metabolic_demand_t*)msg;

    LOG_MODULE_DEBUG("OLIGODENDROCYTE", "Metabolic demand from region %u: glucose=%.2f, ATP deficit=%.2f",
                     demand->region_id, demand->glucose_demand, demand->atp_deficit);

    // Myelination is ATP-intensive - high demand may require reducing myelination rate
    if (demand->atp_deficit > 0.6F) {
        LOG_MODULE_WARN("OLIGODENDROCYTE", "High ATP deficit (%.2f) - reducing myelination activity",
                        demand->atp_deficit);
        bio_router_publish_signal(g_oligo_bio_ctx, "oligodendrocyte.metabolic_constraint", demand->atp_deficit);
    }

    // Oligodendrocytes can provide lactate to axons (metabolic support)
    float lactate_supply = 0.5F; // Moderate lactate support
    bio_router_publish_signal(g_oligo_bio_ctx, "oligodendrocyte.lactate_supply", lactate_supply);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle BIO_MSG_METABOLIC_SUPPLY message
 *
 * Oligodendrocytes receive metabolic support from astrocytes.
 * This allows them to continue myelination activities.
 */
static nimcp_error_t handle_metabolic_supply_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Invalid metabolic supply message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Parse supply amount
    if (msg_size < sizeof(bio_message_header_t) + sizeof(float)) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Metabolic supply message too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
    float supply_amount = *(const float*)payload;

    LOG_MODULE_DEBUG("OLIGODENDROCYTE", "Received metabolic supply: %.2f", supply_amount);

    // With adequate metabolic support, can increase myelination activity
    if (supply_amount > 0.7F) {
        bio_router_publish_signal(g_oligo_bio_ctx, "oligodendrocyte.myelination_boost", supply_amount);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle BIO_MSG_GLIAL_SYNC_REQUEST message
 *
 * Handles synchronization requests for coordinated glial cell activity.
 * Oligodendrocytes coordinate myelination with astrocyte metabolic support.
 */
static nimcp_error_t handle_glial_sync_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Invalid glial sync message");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_MODULE_DEBUG("OLIGODENDROCYTE", "Glial sync request received from module %u", header->source_module);

    // Publish sync acknowledgment
    nimcp_error_t result = bio_router_publish_signal(g_oligo_bio_ctx,
        "oligodendrocyte.sync_ack", 1.0F);

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
static int oligodendrocyte_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_OLIGODENDROCYTE_MYELINATE:
                bio_router_register_handler(ctx, message_types[i], handle_myelination_message);
                registered++;
                break;
            case BIO_MSG_METABOLIC_DEMAND:
                bio_router_register_handler(ctx, message_types[i], handle_metabolic_demand_message);
                registered++;
                break;
            case BIO_MSG_METABOLIC_SUPPLY:
                bio_router_register_handler(ctx, message_types[i], handle_metabolic_supply_message);
                registered++;
                break;
            case BIO_MSG_GLIAL_SYNC_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_glial_sync_message);
                registered++;
                break;
            default:
                LOG_MODULE_DEBUG("OLIGODENDROCYTE", "Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_MODULE_INFO("OLIGODENDROCYTE", "KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

/**
 * @brief Internal initialization function called by nimcp_platform_once
 *
 * Thread-safe initialization of bio-async integration.
 * Result is stored in g_oligo_bio_init_result for callers to check.
 */
static void oligodendrocyte_bio_do_init(void)
{
    // Initialize unified memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    g_oligo_mem_mgr = unified_mem_create(&mem_config);
    if (!g_oligo_mem_mgr) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Failed to create unified memory manager");
        g_oligo_bio_init_result = NIMCP_ERROR_MEMORY;
        return;
    }

    // Register module with bio-router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_OLIGODENDROCYTE,
        .module_name = "Oligodendrocyte",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    g_oligo_bio_ctx = bio_router_register_module(&info);
    if (!g_oligo_bio_ctx) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Failed to register with bio-router");
        unified_mem_destroy(g_oligo_mem_mgr);
        g_oligo_mem_mgr = NULL;
        g_oligo_bio_init_result = NIMCP_ERROR_INVALID_PARAM;
        return;
    }

    // Try KG-driven wiring callback registration first
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_OLIGODENDROCYTE,
        (void*)oligodendrocyte_wiring_handler_callback,
        NULL
    );

    if (result == NIMCP_SUCCESS) {
        LOG_MODULE_INFO("OLIGODENDROCYTE", "KG-driven wiring callback registered successfully");
    } else {
        // Fallback to legacy handler registration
        LOG_MODULE_INFO("OLIGODENDROCYTE", "Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_oligo_bio_ctx,
                                                  BIO_MSG_OLIGODENDROCYTE_MYELINATE,
                                                  handle_myelination_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("OLIGODENDROCYTE", "Failed to register myelination handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_oligo_bio_ctx,
                                                  BIO_MSG_METABOLIC_DEMAND,
                                                  handle_metabolic_demand_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("OLIGODENDROCYTE", "Failed to register metabolic demand handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_oligo_bio_ctx,
                                                  BIO_MSG_METABOLIC_SUPPLY,
                                                  handle_metabolic_supply_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("OLIGODENDROCYTE", "Failed to register metabolic supply handler: %d", result);
            goto cleanup;
        }

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(g_oligo_bio_ctx,
                                                  BIO_MSG_GLIAL_SYNC_REQUEST,
                                                  handle_glial_sync_message)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR("OLIGODENDROCYTE", "Failed to register glial sync handler: %d", result);
            goto cleanup;
        }

        LOG_MODULE_INFO("OLIGODENDROCYTE", "Bio-async integration initialized with legacy handlers (4 handlers registered)");
    }

    g_oligo_bio_init_result = NIMCP_SUCCESS;
    return;

cleanup:
    bio_router_unregister_module(g_oligo_bio_ctx);
    g_oligo_bio_ctx = NULL;
    unified_mem_destroy(g_oligo_mem_mgr);
    g_oligo_mem_mgr = NULL;
    g_oligo_bio_init_result = result;
}

/**
 * @brief Initialize bio-async integration for oligodendrocytes module
 *
 * Uses nimcp_platform_once for thread-safe initialization.
 */
static nimcp_error_t oligodendrocyte_bio_init(void)
{
    nimcp_platform_once(&g_oligo_bio_once, oligodendrocyte_bio_do_init);
    return g_oligo_bio_init_result;
}

/**
 * @brief Shutdown bio-async integration
 */
static void oligodendrocyte_bio_shutdown(void)
{
    // Check if initialization was successful using context pointer
    if (!g_oligo_bio_ctx && !g_oligo_mem_mgr) {
        return;
    }

    LOG_MODULE_INFO("OLIGODENDROCYTE", "Shutting down bio-async integration");

    if (g_oligo_bio_ctx) {
        bio_router_unregister_module(g_oligo_bio_ctx);
        g_oligo_bio_ctx = NULL;
    }

    if (g_oligo_mem_mgr) {
        unified_mem_destroy(g_oligo_mem_mgr);
        g_oligo_mem_mgr = NULL;
    }

    // Reset nimcp_platform_once control for potential re-initialization
    g_oligo_bio_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    g_oligo_bio_init_result = NIMCP_SUCCESS;
}

//=============================================================================
// Public Bio-Async API
//=============================================================================

nimcp_result_t oligodendrocyte_register_bio_handlers(void)
{
    LOG_MODULE_INFO("OLIGODENDROCYTE", "Registering bio-async message handlers");
    return oligodendrocyte_bio_init();
}

void oligodendrocyte_unregister_bio_handlers(void)
{
    LOG_MODULE_INFO("OLIGODENDROCYTE", "Unregistering bio-async message handlers");
    oligodendrocyte_bio_shutdown();
}

//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Find axon index by ID in enhanced array
 *
 * Bounds checking: Validates oligo pointer and axons array before access.
 */
static int32_t find_axon_index(const oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo || !oligo->axons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_axon_index: required parameter is NULL (oligo, oligo->axons)");
        return -1;
    }
    /* Bounds check: num_myelinated_axons should not exceed max_axons */
    if (oligo->num_myelinated_axons > oligo->max_axons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_axon_index: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        if (oligo->axons[i].axon_id == axon_id) {
            return (int32_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_axon_index: validation failed");
    return -1;
}

/**
 * @brief Find neuron index by ID in legacy array
 *
 * Bounds checking: Validates oligo pointer and legacy arrays before access.
 */
static int32_t find_legacy_neuron_index(const oligodendrocyte_t* oligo, uint32_t neuron_id) {
    if (!oligo || !oligo->myelinated_neuron_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_legacy_neuron_index: required parameter is NULL (oligo, oligo->myelinated_neuron_ids)");
        return -1;
    }
    /* Bounds check: num_myelinated_axons should not exceed max_axons */
    if (oligo->num_myelinated_axons > oligo->max_axons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_legacy_neuron_index: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        if (oligo->myelinated_neuron_ids[i] == neuron_id) {
            return (int32_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_legacy_neuron_index: validation failed");
    return -1;
}

/**
 * @brief RK4 derivative computation for oligodendrocyte state
 *
 * STATE VARIABLES:
 * - state[0]: Myelination rate (driven by signals)
 * - state[1]: Activity integration (EMA of axon activity)
 * - state[2]: Energy state (ATP dynamics)
 * - state[3]: Maturation progress (OPC → mature)
 */
static void compute_state_derivatives(const oligodendrocyte_t* oligo,
                                       const float* state,
                                       float* derivatives) {
    /* Defensive null checks for state arrays */
    if (!state || !derivatives) {
        if (derivatives) {
            derivatives[0] = derivatives[1] = derivatives[2] = derivatives[3] = 0.0F;
        }
        return;
    }

    /* Extract state components */
    float myelin_rate = state[0];
    float activity_int = state[1];
    float energy = state[2];
    float maturation = state[3];

    /* Compute total activity from axons with bounds validation */
    float total_activity = 0.0F;
    if (oligo->num_myelinated_axons > 0 && oligo->axons &&
        oligo->num_myelinated_axons <= oligo->max_axons) {
        for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
            total_activity += oligo->axons[i].activity_score;
        }
        total_activity /= oligo->num_myelinated_axons;
    }

    // Compute growth factor signal
    float gf_signal = oligo->growth_factors.concentrations[GROWTH_FACTOR_NRG1] *
                      NIMCP_NRG1_MYELIN_COEFFICIENT +
                      oligo->growth_factors.concentrations[GROWTH_FACTOR_BDNF] *
                      NIMCP_BDNF_MYELIN_COEFFICIENT;

    // d(myelin_rate)/dt: Driven by activity and growth factors, modulated by energy
    float target_rate = (total_activity + gf_signal) * energy;
    derivatives[0] = (target_rate - myelin_rate) / NIMCP_OLIGO_STATE_TAU_S;

    // d(activity_int)/dt: Exponential moving average
    derivatives[1] = (total_activity - activity_int) / NIMCP_OLIGO_ACTIVITY_TAU_S;

    // d(energy)/dt: ATP dynamics
    float total_myelin = 0.0F;
    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        total_myelin += oligo->axons[i].myelination_level;
    }
    float cost = total_myelin * NIMCP_OLIGO_ATP_COST_PER_MYELIN +
                 oligo->lactate_shuttle.production_rate * 0.01F;
    float regen = NIMCP_OLIGO_ATP_REGEN_RATE + oligo->glucose_level * NIMCP_OLIGO_GLUCOSE_UPTAKE_RATE;
    derivatives[2] = regen - cost;

    // d(maturation)/dt: Progress based on activity and time
    float maturation_target = 0.0F;
    if (total_activity > NIMCP_OLIGO_ACTIVITY_THRESHOLD_HZ) {
        maturation_target = 1.0F;
    }
    derivatives[3] = (maturation_target - maturation) / (NIMCP_OLIGO_STATE_TAU_S * 10.0F);
}

/**
 * @brief RK4 integration step
 */
static void rk4_step(oligodendrocyte_t* oligo, float dt) {
    float k1[4], k2[4], k3[4], k4[4];
    float temp_state[4];

    // k1 = f(state)
    compute_state_derivatives(oligo, oligo->state_variables, k1);

    // k2 = f(state + dt/2 * k1)
    for (int i = 0; i < 4; i++) {
        temp_state[i] = oligo->state_variables[i] + 0.5F * dt * k1[i];
    }
    compute_state_derivatives(oligo, temp_state, k2);

    // k3 = f(state + dt/2 * k2)
    for (int i = 0; i < 4; i++) {
        temp_state[i] = oligo->state_variables[i] + 0.5F * dt * k2[i];
    }
    compute_state_derivatives(oligo, temp_state, k3);

    // k4 = f(state + dt * k3)
    for (int i = 0; i < 4; i++) {
        temp_state[i] = oligo->state_variables[i] + dt * k3[i];
    }
    compute_state_derivatives(oligo, temp_state, k4);

    // Update: state += dt/6 * (k1 + 2*k2 + 2*k3 + k4)
    for (int i = 0; i < 4; i++) {
        oligo->state_variables[i] += (dt / 6.0F) *
            (k1[i] + 2.0F * k2[i] + 2.0F * k3[i] + k4[i]);
    }

    // Clamp state variables
    oligo->state_variables[0] = nimcp_clampf(oligo->state_variables[0], 0.0F, NIMCP_OLIGO_MAX_MYELIN_RATE);
    oligo->state_variables[1] = nimcp_clampf(oligo->state_variables[1], 0.0F, 100.0F);
    oligo->state_variables[2] = nimcp_clampf(oligo->state_variables[2], 0.0F, NIMCP_OLIGO_ATP_MAX);
    oligo->state_variables[3] = nimcp_clampf(oligo->state_variables[3], 0.0F, 1.0F);

    // Update derived properties
    oligo->myelination_rate = oligo->state_variables[0];
    oligo->atp_level = oligo->state_variables[2];
    oligo->maturation_progress = oligo->state_variables[3];
}

/**
 * @brief Compute G-ratio efficiency factor for conduction velocity
 *
 * Optimal G-ratio is around 0.7 for CNS axons.
 * Efficiency decreases as G-ratio deviates from optimal.
 */
static float compute_g_ratio_efficiency(float g_ratio) {
    float optimal = NIMCP_OLIGO_OPTIMAL_G_RATIO;
    float deviation = fabsf(g_ratio - optimal);
    // Gaussian-like efficiency curve
    float efficiency = expf(-deviation * deviation / (2.0F * 0.1F * 0.1F));
    return nimcp_clampf(efficiency, 0.5F, 1.0F);
}

/**
 * @brief Determine myelin state from myelination level
 */
static myelin_state_t compute_myelin_state(float level) {
    if (level < 0.05F) return MYELIN_STATE_UNMYELINATED;
    if (level < 0.2F) return MYELIN_STATE_INITIATING;
    if (level < 0.8F) return MYELIN_STATE_PARTIAL;
    return MYELIN_STATE_MATURE;
}

/**
 * @brief Initialize internode segments for an axon
 */
static void initialize_internodes(myelinated_axon_t* axon) {
    if (!axon || axon->axon_length <= 0.0F) return;

    // Compute optimal internode length based on axon diameter
    float optimal_length = axon->axon_diameter * NIMCP_OLIGO_INTERNODE_DIAMETER_RATIO;
    optimal_length = nimcp_clampf(optimal_length, NIMCP_OLIGO_INTERNODE_MIN_UM, NIMCP_OLIGO_INTERNODE_MAX_UM);

    // Number of internodes
    uint32_t num_nodes = (uint32_t)(axon->axon_length / optimal_length);
    if (num_nodes < 1) num_nodes = 1;
    if (num_nodes > axon->max_internodes) num_nodes = axon->max_internodes;

    // Initialize internode segments
    float position = 0.0F;
    float segment_length = axon->axon_length / (float)num_nodes;

    for (uint32_t i = 0; i < num_nodes; i++) {
        axon->internodes[i].start_position = position;
        axon->internodes[i].length = segment_length;
        axon->internodes[i].myelin_thickness = 0.0F;
        axon->internodes[i].g_ratio = 1.0F; // No myelin initially
        axon->internodes[i].wrap_count = 0;
        axon->internodes[i].compaction = 0.0F;
        position += segment_length + NIMCP_OLIGO_NODE_LENGTH_UM;
    }

    axon->num_internodes = num_nodes;
    axon->total_myelin_length = 0.0F;
}

//=============================================================================
// CREATION & DESTRUCTION
//=============================================================================

oligodendrocyte_t* oligodendrocyte_create(uint32_t id, float x, float y, float z,
                                           uint32_t max_axons) {
    // Initialize bio-async on first create (thread-safe via nimcp_platform_once)
    nimcp_error_t init_result = oligodendrocyte_bio_init();
    if (init_result != NIMCP_SUCCESS) {
        LOG_MODULE_WARN("OLIGODENDROCYTE", "Bio-async init failed: %d (continuing anyway)", init_result);
    }

    if (max_axons == 0 || max_axons > NIMCP_OLIGO_MAX_AXONS) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Invalid max_axons: %u", max_axons);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_internodes: max_axons is zero");
        return NULL;
    }

    oligodendrocyte_t* oligo = (oligodendrocyte_t*)nimcp_malloc(sizeof(oligodendrocyte_t));
    if (!oligo) {
        LOG_MODULE_ERROR("OLIGODENDROCYTE", "Failed to allocate oligodendrocyte structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "initialize_internodes: oligo is NULL");
        return NULL;
    }

    memset(oligo, 0, sizeof(oligodendrocyte_t));

    oligo->id = id;
    oligo->max_axons = max_axons;
    oligo->num_myelinated_axons = 0;

    // Spatial position
    oligo->position[0] = x;
    oligo->position[1] = y;
    oligo->position[2] = z;
    oligo->process_reach = 100.0F; // Default 100 µm reach
    oligo->territory_radius = 150.0F; // Default territory

    // Maturation state
    oligo->maturation = OLIGO_STATE_OPC;
    oligo->maturation_progress = 0.0F;
    oligo->maturation_time = nimcp_time_monotonic_us();

    // Initialize state variables for RK4
    oligo->state_variables[0] = 0.0F; // Myelination rate
    oligo->state_variables[1] = 0.0F; // Activity integration
    oligo->state_variables[2] = 1.0F; // Energy (ATP)
    oligo->state_variables[3] = 0.0F; // Maturation progress
    oligo->myelination_rate = 0.0F;

    // Allocate enhanced axon array
    oligo->axons = (myelinated_axon_t*)nimcp_malloc(max_axons * sizeof(myelinated_axon_t));
    if (!oligo->axons) {
        oligodendrocyte_destroy(oligo);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_internodes: oligo->axons is NULL");
        return NULL;
    }
    memset(oligo->axons, 0, max_axons * sizeof(myelinated_axon_t));

    // Allocate internodes for each axon (default 10 segments max)
    for (uint32_t i = 0; i < max_axons; i++) {
        oligo->axons[i].max_internodes = 10;
        oligo->axons[i].internodes = (internode_segment_t*)nimcp_malloc(
            10 * sizeof(internode_segment_t));
        if (!oligo->axons[i].internodes) {
            oligodendrocyte_destroy(oligo);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_internodes: oligo->axons is NULL");
            return NULL;
        }
        memset(oligo->axons[i].internodes, 0, 10 * sizeof(internode_segment_t));
    }

    // Allocate legacy arrays for backward compatibility
    oligo->myelinated_neuron_ids = (uint32_t*)nimcp_malloc(max_axons * sizeof(uint32_t));
    oligo->myelination_levels = (float*)nimcp_malloc(max_axons * sizeof(float));
    oligo->neuron_activity_history = (float*)nimcp_malloc(max_axons * sizeof(float));
    oligo->last_spike_times = (uint64_t*)nimcp_malloc(max_axons * sizeof(uint64_t));

    if (!oligo->myelinated_neuron_ids || !oligo->myelination_levels ||
        !oligo->neuron_activity_history || !oligo->last_spike_times) {
        oligodendrocyte_destroy(oligo);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "initialize_internodes: operation failed");
        return NULL;
    }

    // Initialize legacy arrays
    for (uint32_t i = 0; i < max_axons; i++) {
        oligo->myelinated_neuron_ids[i] = UINT32_MAX;
        oligo->myelination_levels[i] = 0.0F;
        oligo->neuron_activity_history[i] = 0.0F;
        oligo->last_spike_times[i] = 0;
    }

    // Initialize growth factor state
    for (int i = 0; i < NIMCP_GROWTH_FACTOR_COUNT; i++) {
        oligo->growth_factors.concentrations[i] = 0.0F;
        oligo->growth_factors.production_rates[i] = 0.0F;
        oligo->growth_factors.reception_rates[i] = 1.0F; // Full sensitivity
    }
    oligo->growth_factors.last_update_time = nimcp_time_monotonic_us();

    // Initialize lactate shuttle
    oligo->lactate_shuttle.lactate_pool = 0.5F; // Start with half-full pool
    oligo->lactate_shuttle.production_rate = NIMCP_OLIGO_LACTATE_MAX_PRODUCTION * 0.5F;
    oligo->lactate_shuttle.transfer_rate = 0.0F;
    oligo->lactate_shuttle.glucose_uptake = NIMCP_OLIGO_GLUCOSE_UPTAKE_RATE;
    oligo->lactate_shuttle.supported_axon_count = 0;
    oligo->lactate_shuttle.axon_lactate_delivery = (float*)nimcp_malloc(max_axons * sizeof(float));
    if (!oligo->lactate_shuttle.axon_lactate_delivery) {
        oligodendrocyte_destroy(oligo);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_internodes: oligo->lactate_shuttle is NULL");
        return NULL;
    }
    memset(oligo->lactate_shuttle.axon_lactate_delivery, 0, max_axons * sizeof(float));

    // Metabolic state
    oligo->atp_level = 1.0F;
    oligo->metabolic_cost = 0.0F;
    oligo->max_myelination_capacity = (float)max_axons;
    oligo->glucose_level = 0.5F;

    // Remodeling state
    oligo->last_remodeling_time = nimcp_time_monotonic_us();
    oligo->remodeling_interval_ms = NIMCP_OLIGO_REMODEL_TAU_S * 1000.0F;
    oligo->g_ratio_optimization_rate = 0.1F;

    // Statistics
    oligo->total_myelin_segments = 0;
    oligo->total_myelin_volume = 0.0F;
    oligo->avg_g_ratio = NIMCP_OLIGO_G_RATIO_MAX;
    oligo->avg_conduction_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS;
    oligo->demyelination_events = 0;
    oligo->total_lactate_delivered = 0.0F;

    // Initialize lock
    nimcp_spinlock_init(&oligo->lock);

    // Initialize CoW fields (Phase 1.5+)
    oligo->cow_ref_count = 1;
    oligo->cow_modified = false;
    oligo->cow_original = NULL;

    return oligo;
}

oligodendrocyte_t* oligodendrocyte_create_basic(uint32_t id, uint32_t max_axons) {
    return oligodendrocyte_create(id, 0.0F, 0.0F, 0.0F, max_axons);
}

void oligodendrocyte_destroy(oligodendrocyte_t* oligo) {
    if (!oligo) return;

    // Free enhanced axon data
    if (oligo->axons) {
        for (uint32_t i = 0; i < oligo->max_axons; i++) {
            if (oligo->axons[i].internodes) {
                nimcp_free(oligo->axons[i].internodes);
            }
        }
        nimcp_free(oligo->axons);
    }

    // Free legacy arrays
    if (oligo->myelinated_neuron_ids) nimcp_free(oligo->myelinated_neuron_ids);
    if (oligo->myelination_levels) nimcp_free(oligo->myelination_levels);
    if (oligo->neuron_activity_history) nimcp_free(oligo->neuron_activity_history);
    if (oligo->last_spike_times) nimcp_free(oligo->last_spike_times);

    // Free lactate shuttle data
    if (oligo->lactate_shuttle.axon_lactate_delivery) {
        nimcp_free(oligo->lactate_shuttle.axon_lactate_delivery);
    }

    nimcp_free(oligo);
}

oligodendrocyte_network_config_t oligodendrocyte_network_default_config(void) {
    oligodendrocyte_network_config_t config;
    config.capacity = 100;
    config.max_axons_per_oligo = NIMCP_OLIGO_MAX_AXONS;
    config.activity_threshold = NIMCP_OLIGO_ACTIVITY_THRESHOLD_HZ;
    config.territory_radius = 150.0F;
    config.target_g_ratio = NIMCP_OLIGO_OPTIMAL_G_RATIO;
    config.enable_g_ratio_optimization = true;
    config.enable_growth_factor_signaling = true;
    config.enable_lactate_shuttle = true;
    config.enable_state_dynamics = true;
    config.enable_centrality_priority = true;
    config.filter_cutoff_hz = 10.0F;
    return config;
}

oligodendrocyte_network_t* oligodendrocyte_network_create_enhanced(
    const oligodendrocyte_network_config_t* config) {
    if (!config || config->capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligodendrocyte_network_default_config: config is NULL");
        return NULL;
    }

    oligodendrocyte_network_t* network =
        (oligodendrocyte_network_t*)nimcp_malloc(sizeof(oligodendrocyte_network_t));
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    memset(network, 0, sizeof(oligodendrocyte_network_t));

    network->capacity = config->capacity;
    network->num_oligodendrocytes = 0;

    network->oligodendrocytes =
        (oligodendrocyte_t**)nimcp_malloc(config->capacity * sizeof(oligodendrocyte_t*));
    if (!network->oligodendrocytes) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligodendrocyte_network_default_config: network->oligodendrocytes is NULL");
        return NULL;
    }
    memset(network->oligodendrocytes, 0, config->capacity * sizeof(oligodendrocyte_t*));

    // Spatial indexing (initialized on first use)
    network->oligo_tree = NULL;
    network->axon_tree = NULL;
    network->spatial_index_valid = false;

    // Growth factor field (initialized on first use)
    network->global_growth_factor_field = NULL;
    network->growth_factor_field_size = 0;

    // Centrality scores (initialized on first use)
    network->axon_centrality = NULL;
    network->num_centrality_scores = 0;
    network->centrality_valid = false;

    // Global parameters
    network->base_conduction_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS;
    network->myelinated_velocity_multiplier = NIMCP_OLIGO_MYELIN_MULTIPLIER;
    network->activity_threshold = config->activity_threshold;
    network->global_g_ratio_target = config->target_g_ratio;

    // Activity filter
    network->filter_cutoff_hz = config->filter_cutoff_hz;
    network->filter_alpha = 1.0F / (1.0F + 1.0F / (2.0F * 3.14159F * config->filter_cutoff_hz * 0.001F));

    nimcp_mutex_init(&network->lock, NULL);

    // Create memory pools (Phase 1.5+)
    network->axon_pool = oligo_axon_pool_create(NIMCP_OLIGO_AXON_POOL_SIZE);
    network->internode_pool = oligo_internode_pool_create(NIMCP_OLIGO_INTERNODE_POOL_SIZE);

    return network;
}

oligodendrocyte_network_t* oligodendrocyte_network_create(uint32_t capacity) {
    oligodendrocyte_network_config_t config = oligodendrocyte_network_default_config();
    config.capacity = capacity;
    return oligodendrocyte_network_create_enhanced(&config);
}

void oligodendrocyte_network_destroy(oligodendrocyte_network_t* network) {
    if (!network) return;

    // Destroy all oligodendrocytes
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        if (network->oligodendrocytes[i]) {
            oligodendrocyte_destroy(network->oligodendrocytes[i]);
        }
    }

    if (network->oligodendrocytes) nimcp_free(network->oligodendrocytes);
    if (network->oligo_tree) kdtree_destroy(network->oligo_tree);
    if (network->axon_tree) kdtree_destroy(network->axon_tree);
    if (network->global_growth_factor_field) nimcp_free(network->global_growth_factor_field);
    if (network->axon_centrality) nimcp_free(network->axon_centrality);

    // Destroy memory pools (Phase 1.5+)
    if (network->axon_pool) oligo_axon_pool_destroy(network->axon_pool);
    if (network->internode_pool) oligo_internode_pool_destroy(network->internode_pool);

    nimcp_mutex_destroy(&network->lock);
    nimcp_free(network);
}

//=============================================================================
// AXON ASSIGNMENT & MYELINATION
//=============================================================================

nimcp_result_t oligodendrocyte_assign_axon_at(oligodendrocyte_t* oligo,
                                               uint32_t axon_id,
                                               float x, float y, float z,
                                               float axon_diameter,
                                               float axon_length) {
    NIMCP_CHECK_THROW(oligo, NIMCP_ERROR_INVALID_PARAM, "oligo is NULL");

    nimcp_spinlock_lock(&oligo->lock);

    // Check for duplicate
    int32_t existing = find_axon_index(oligo, axon_id);
    if (existing >= 0) {
        nimcp_spinlock_unlock(&oligo->lock);
        return NIMCP_SUCCESS; // Already assigned
    }

    // Check capacity
    if (oligo->num_myelinated_axons >= oligo->max_axons) {
        nimcp_spinlock_unlock(&oligo->lock);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Assign to enhanced array
    uint32_t idx = oligo->num_myelinated_axons;
    myelinated_axon_t* axon = &oligo->axons[idx];

    axon->axon_id = axon_id;
    axon->position[0] = x;
    axon->position[1] = y;
    axon->position[2] = z;

    // Myelination state
    axon->myelin_state = MYELIN_STATE_UNMYELINATED;
    axon->myelination_level = 0.0F;
    axon->target_myelination = 0.0F;

    // G-ratio parameters
    axon->axon_diameter = axon_diameter > 0.0F ? axon_diameter : 1.0F;
    axon->fiber_diameter = axon->axon_diameter; // No myelin initially
    axon->g_ratio = 1.0F; // Unmyelinated = 1.0
    axon->optimal_g_ratio = oligodendrocyte_compute_optimal_g_ratio(axon->axon_diameter, 0.0F);

    // Internode segments
    axon->axon_length = axon_length > 0.0F ? axon_length : 100.0F;
    initialize_internodes(axon);

    // Activity tracking
    axon->activity_score = 0.0F;
    axon->filtered_activity = 0.0F;
    axon->last_activity_time = 0;
    axon->activity_integral = 0.0F;

    // Growth factor sensitivity (default full sensitivity)
    axon->nrg1_sensitivity = 1.0F;
    axon->bdnf_sensitivity = 1.0F;

    // Centrality
    axon->centrality_score = 0.0F;
    axon->priority_myelination = false;

    // Conduction properties
    axon->conduction_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS;
    axon->conduction_delay = axon->axon_length / (1000.0F * axon->conduction_velocity);

    // Metabolic support
    axon->lactate_received = 0.0F;
    axon->metabolic_demand = 1.0F;

    // Update legacy arrays
    oligo->myelinated_neuron_ids[idx] = axon_id;
    oligo->myelination_levels[idx] = 0.0F;
    oligo->neuron_activity_history[idx] = 0.0F;
    oligo->last_spike_times[idx] = 0;

    oligo->num_myelinated_axons++;

    nimcp_spinlock_unlock(&oligo->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t oligodendrocyte_assign_neuron(oligodendrocyte_t* oligo, uint32_t neuron_id) {
    // Use default parameters for backward compatibility
    return oligodendrocyte_assign_axon_at(oligo, neuron_id, 0.0F, 0.0F, 0.0F, 1.0F, 100.0F);
}

float oligodendrocyte_get_myelination_level(oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo) return 0.0F;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    float level = (idx >= 0) ? oligo->axons[idx].myelination_level : 0.0F;

    nimcp_spinlock_unlock(&oligo->lock);
    return level;
}

nimcp_result_t oligodendrocyte_set_myelination_level(oligodendrocyte_t* oligo,
                                                      uint32_t axon_id,
                                                      float level) {
    NIMCP_CHECK_THROW(oligo, NIMCP_ERROR_INVALID_PARAM, "oligo is NULL");

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    if (idx < 0) {
        nimcp_spinlock_unlock(&oligo->lock);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    level = nimcp_clampf(level, 0.0F, 1.0F);
    oligo->axons[idx].myelination_level = level;
    oligo->axons[idx].myelin_state = compute_myelin_state(level);

    // Update legacy array
    oligo->myelination_levels[idx] = level;

    // Update G-ratio based on myelination level
    if (level > 0.0F) {
        // G-ratio decreases (more myelin) as myelination increases
        oligo->axons[idx].g_ratio = 1.0F - level * (1.0F - oligo->axons[idx].optimal_g_ratio);
        oligo->axons[idx].g_ratio = nimcp_clampf(oligo->axons[idx].g_ratio,
                                           NIMCP_OLIGO_G_RATIO_MIN,
                                           NIMCP_OLIGO_G_RATIO_MAX);

        // Update fiber diameter
        float myelin_thickness = oligo->axons[idx].axon_diameter *
                                 (1.0F / oligo->axons[idx].g_ratio - 1.0F) / 2.0F;
        oligo->axons[idx].fiber_diameter = oligo->axons[idx].axon_diameter + 2.0F * myelin_thickness;
    } else {
        oligo->axons[idx].g_ratio = 1.0F;
        oligo->axons[idx].fiber_diameter = oligo->axons[idx].axon_diameter;
    }

    nimcp_spinlock_unlock(&oligo->lock);
    return NIMCP_SUCCESS;
}

myelin_state_t oligodendrocyte_get_myelin_state(oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo) return MYELIN_STATE_UNMYELINATED;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    myelin_state_t state = (idx >= 0) ? oligo->axons[idx].myelin_state : MYELIN_STATE_UNMYELINATED;

    nimcp_spinlock_unlock(&oligo->lock);
    return state;
}

//=============================================================================
// G-RATIO OPTIMIZATION
//=============================================================================

float oligodendrocyte_get_g_ratio(oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo) return -1.0F;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    float g_ratio = (idx >= 0) ? oligo->axons[idx].g_ratio : -1.0F;

    nimcp_spinlock_unlock(&oligo->lock);
    return g_ratio;
}

float oligodendrocyte_compute_optimal_g_ratio(float axon_diameter, float activity_level) {
    // Rushton's optimization: optimal G-ratio varies with axon diameter
    // Small axons: 0.6, Large axons: 0.77
    float base_optimal = 0.6F + 0.17F * (1.0F - expf(-axon_diameter / 5.0F));

    // Activity modulation: higher activity favors slightly thinner myelin for plasticity
    float activity_mod = activity_level / 100.0F; // Normalize activity
    float optimal = base_optimal + activity_mod * 0.05F;

    return nimcp_clampf(optimal, NIMCP_OLIGO_G_RATIO_MIN, NIMCP_OLIGO_G_RATIO_MAX);
}

void oligodendrocyte_optimize_g_ratios(oligodendrocyte_t* oligo, float dt) {
    if (!oligo || dt <= 0.0F) return;

    nimcp_spinlock_lock(&oligo->lock);

    float rate = dt / NIMCP_OLIGO_G_RATIO_TAU_S;
    float atp_factor = nimcp_clampf(oligo->atp_level, 0.1F, 1.0F);
    rate *= atp_factor;

    float total_g_ratio = 0.0F;
    uint32_t myelinated_count = 0;

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        myelinated_axon_t* axon = &oligo->axons[i];

        if (axon->myelination_level > 0.0F) {
            // Update optimal G-ratio based on current activity
            axon->optimal_g_ratio = oligodendrocyte_compute_optimal_g_ratio(
                axon->axon_diameter, axon->activity_score);

            // Move current G-ratio toward optimal
            float target = axon->optimal_g_ratio;
            float delta = (target - axon->g_ratio) * rate;
            axon->g_ratio += delta;
            axon->g_ratio = nimcp_clampf(axon->g_ratio, NIMCP_OLIGO_G_RATIO_MIN, NIMCP_OLIGO_G_RATIO_MAX);

            // Update fiber diameter
            float myelin_thickness = axon->axon_diameter * (1.0F / axon->g_ratio - 1.0F) / 2.0F;
            axon->fiber_diameter = axon->axon_diameter + 2.0F * myelin_thickness;

            // Update internode segments
            for (uint32_t j = 0; j < axon->num_internodes; j++) {
                axon->internodes[j].g_ratio = axon->g_ratio;
                axon->internodes[j].myelin_thickness = myelin_thickness;
            }

            total_g_ratio += axon->g_ratio;
            myelinated_count++;
        }
    }

    // Update average G-ratio statistic
    if (myelinated_count > 0) {
        oligo->avg_g_ratio = total_g_ratio / (float)myelinated_count;
    }

    nimcp_spinlock_unlock(&oligo->lock);
}

float oligodendrocyte_get_g_ratio_deviation(oligodendrocyte_t* oligo) {
    if (!oligo) return 0.0F;

    nimcp_spinlock_lock(&oligo->lock);

    float total_deviation = 0.0F;
    uint32_t count = 0;

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        if (oligo->axons[i].myelination_level > 0.0F) {
            total_deviation += fabsf(oligo->axons[i].g_ratio - oligo->axons[i].optimal_g_ratio);
            count++;
        }
    }

    nimcp_spinlock_unlock(&oligo->lock);
    return (count > 0) ? total_deviation / (float)count : 0.0F;
}

//=============================================================================
// SALTATORY CONDUCTION
//=============================================================================

float oligodendrocyte_compute_conduction_velocity(oligodendrocyte_t* oligo,
                                                   uint32_t axon_id,
                                                   float base_velocity) {
    if (!oligo) return base_velocity;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    if (idx < 0) {
        nimcp_spinlock_unlock(&oligo->lock);
        return base_velocity;
    }

    float velocity = oligodendrocyte_compute_saltatory_velocity(&oligo->axons[idx]);
    if (velocity < base_velocity) velocity = base_velocity;

    // Update stored value
    oligo->axons[idx].conduction_velocity = velocity;
    oligo->axons[idx].conduction_delay = oligo->axons[idx].axon_length / (1000.0F * velocity);

    nimcp_spinlock_unlock(&oligo->lock);
    return velocity;
}

float oligodendrocyte_compute_saltatory_velocity(const myelinated_axon_t* axon) {
    if (!axon) return NIMCP_OLIGO_BASE_VELOCITY_MS;

    if (axon->myelination_level < 0.01F) {
        // Unmyelinated: continuous conduction
        return NIMCP_OLIGO_BASE_VELOCITY_MS;
    }

    // Rushton's law for saltatory conduction:
    // v = k × d (where k ≈ 5.5 m/s per µm for myelinated axons)
    // Modified by G-ratio efficiency and myelination completeness

    float k = 5.5F; // Conduction constant (m/s per µm diameter)
    float diameter_factor = axon->axon_diameter;

    // G-ratio efficiency factor (optimal around 0.7)
    float g_efficiency = compute_g_ratio_efficiency(axon->g_ratio);

    // Myelination completeness factor
    float myelin_factor = axon->myelination_level;

    // Internode efficiency (longer internodes = faster, but diminishing returns)
    float avg_internode_length = 0.0F;
    if (axon->num_internodes > 0) {
        float total_length = 0.0F;
        for (uint32_t i = 0; i < axon->num_internodes; i++) {
            total_length += axon->internodes[i].length;
        }
        avg_internode_length = total_length / (float)axon->num_internodes;
    }
    float optimal_internode = axon->axon_diameter * NIMCP_OLIGO_INTERNODE_DIAMETER_RATIO;
    float internode_factor = 1.0F - 0.3F * fabsf(avg_internode_length - optimal_internode) / optimal_internode;
    internode_factor = nimcp_clampf(internode_factor, 0.5F, 1.0F);

    // Combined velocity calculation
    float velocity = k * diameter_factor * g_efficiency * myelin_factor * internode_factor;

    // Apply base velocity minimum
    velocity = fmaxf(velocity, NIMCP_OLIGO_BASE_VELOCITY_MS);

    // Cap at maximum myelinated velocity
    float max_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS * NIMCP_OLIGO_MYELIN_MULTIPLIER;
    velocity = fminf(velocity, max_velocity);

    return velocity;
}

float oligodendrocyte_compute_propagation_delay(oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo) return 0.0F;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    float delay = 0.0F;
    if (idx >= 0) {
        float velocity = oligodendrocyte_compute_saltatory_velocity(&oligo->axons[idx]);
        delay = oligo->axons[idx].axon_length / (1000.0F * velocity); // ms
    }

    nimcp_spinlock_unlock(&oligo->lock);
    return delay;
}

void oligodendrocyte_optimize_internode_spacing(oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo) return;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    if (idx >= 0) {
        myelinated_axon_t* axon = &oligo->axons[idx];

        // Optimal internode length ≈ 100 × axon diameter
        float optimal_length = axon->axon_diameter * NIMCP_OLIGO_INTERNODE_DIAMETER_RATIO;
        optimal_length = nimcp_clampf(optimal_length, NIMCP_OLIGO_INTERNODE_MIN_UM, NIMCP_OLIGO_INTERNODE_MAX_UM);

        // Gradually adjust existing internodes toward optimal
        for (uint32_t i = 0; i < axon->num_internodes; i++) {
            float current = axon->internodes[i].length;
            float delta = (optimal_length - current) * 0.1F; // 10% adjustment per call
            axon->internodes[i].length = nimcp_clampf(current + delta,
                                                 NIMCP_OLIGO_INTERNODE_MIN_UM,
                                                 NIMCP_OLIGO_INTERNODE_MAX_UM);
        }
    }

    nimcp_spinlock_unlock(&oligo->lock);
}

//=============================================================================
// ACTIVITY TRACKING & ADAPTIVE MYELINATION
//=============================================================================

void oligodendrocyte_track_activity(oligodendrocyte_t* oligo,
                                     uint32_t axon_id,
                                     float activity,
                                     uint64_t timestamp) {
    if (!oligo) return;

    activity = nimcp_clampf(activity, 0.0F, 100.0F);

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    if (idx >= 0) {
        myelinated_axon_t* axon = &oligo->axons[idx];

        // Update EMA for activity score
        float alpha = 0.3F;
        axon->activity_score = alpha * activity + (1.0F - alpha) * axon->activity_score;

        // Update filtered activity (lower alpha for smoother filtering)
        float filter_alpha = 0.1F;
        axon->filtered_activity = filter_alpha * activity + (1.0F - filter_alpha) * axon->filtered_activity;

        // Update activity integral (guard against backward timestamps)
        if (axon->last_activity_time > 0 && timestamp > axon->last_activity_time) {
            float dt = (float)(timestamp - axon->last_activity_time) / 1e6F; // seconds
            axon->activity_integral += activity * dt;
        }

        axon->last_activity_time = timestamp;

        // Update legacy arrays
        oligo->neuron_activity_history[idx] = axon->activity_score;
        oligo->last_spike_times[idx] = timestamp;
    }

    nimcp_spinlock_unlock(&oligo->lock);
}

void oligodendrocyte_update_activity_scores(oligodendrocyte_t* oligo, uint64_t current_time) {
    if (!oligo) return;

    nimcp_spinlock_lock(&oligo->lock);

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        myelinated_axon_t* axon = &oligo->axons[i];

        // Apply decay based on time since last activity
        if (axon->last_activity_time > 0 && current_time > axon->last_activity_time) {
            float dt = (float)(current_time - axon->last_activity_time) / 1e6F;
            float decay = expf(-dt / NIMCP_OLIGO_ACTIVITY_TAU_S);
            axon->activity_score *= decay;
            axon->filtered_activity *= decay;

            // Update legacy array
            oligo->neuron_activity_history[i] = axon->activity_score;
        }
    }

    nimcp_spinlock_unlock(&oligo->lock);
}

void oligodendrocyte_remodel_myelination(oligodendrocyte_t* oligo, float dt) {
    if (!oligo || dt <= 0.0F) return;

    nimcp_spinlock_lock(&oligo->lock);

    float rate = dt / NIMCP_OLIGO_REMODEL_TAU_S;
    float atp_factor = nimcp_clampf(oligo->atp_level, 0.1F, 1.0F);
    rate *= atp_factor;

    // Compute growth factor signal
    float gf_signal = oligo->growth_factors.concentrations[GROWTH_FACTOR_NRG1] *
                      NIMCP_NRG1_MYELIN_COEFFICIENT +
                      oligo->growth_factors.concentrations[GROWTH_FACTOR_BDNF] *
                      NIMCP_BDNF_MYELIN_COEFFICIENT;

    float total_myelin = 0.0F;
    float total_velocity = 0.0F;

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        myelinated_axon_t* axon = &oligo->axons[i];

        // Compute target myelination based on activity and signals
        float activity_factor = nimcp_clampf(axon->activity_score / 10.0F, 0.0F, 1.0F);
        float centrality_factor = 1.0F + axon->centrality_score * NIMCP_OLIGO_CENTRALITY_PRIORITY_FACTOR;

        // Scale growth factor response by individual axon activity to preserve differentiation
        float signal_factor = gf_signal * axon->nrg1_sensitivity * activity_factor;

        // Activity-weighted target: activity drives myelination, signals amplify it
        float base_target = activity_factor * (1.0F + signal_factor * 0.3F) * centrality_factor;

        // Preserve existing myelination: established myelin is stable without activity
        // Only decay slowly if there's existing myelin but low activity
        if (axon->myelination_level > base_target) {
            // Decay is much slower than growth (myelin maintenance is metabolically efficient)
            axon->target_myelination = axon->myelination_level * 0.99F + base_target * 0.01F;
            if (!isfinite(axon->target_myelination)) axon->target_myelination = base_target;
        } else {
            axon->target_myelination = base_target;
        }
        axon->target_myelination = nimcp_clampf(axon->target_myelination, 0.0F, 1.0F);

        // Move current myelination toward target
        float delta = (axon->target_myelination - axon->myelination_level) * rate;
        axon->myelination_level += delta;
        axon->myelination_level = nimcp_clampf(axon->myelination_level, 0.0F, 1.0F);

        // Update myelin state
        axon->myelin_state = compute_myelin_state(axon->myelination_level);

        // Update G-ratio based on new myelination level
        if (axon->myelination_level > 0.0F) {
            axon->g_ratio = 1.0F - axon->myelination_level * (1.0F - axon->optimal_g_ratio);
            axon->g_ratio = nimcp_clampf(axon->g_ratio, NIMCP_OLIGO_G_RATIO_MIN, NIMCP_OLIGO_G_RATIO_MAX);

            float myelin_thickness = axon->axon_diameter * (1.0F / axon->g_ratio - 1.0F) / 2.0F;
            axon->fiber_diameter = axon->axon_diameter + 2.0F * myelin_thickness;
        } else {
            axon->g_ratio = 1.0F;
            axon->fiber_diameter = axon->axon_diameter;
        }

        // Update conduction velocity
        axon->conduction_velocity = oligodendrocyte_compute_saltatory_velocity(axon);
        axon->conduction_delay = axon->axon_length / (1000.0F * axon->conduction_velocity);

        // Update legacy array
        oligo->myelination_levels[i] = axon->myelination_level;

        total_myelin += axon->myelination_level;
        total_velocity += axon->conduction_velocity;
    }

    // Enforce capacity constraint
    if (total_myelin > oligo->max_myelination_capacity && oligo->num_myelinated_axons > 0) {
        float scale = oligo->max_myelination_capacity / total_myelin;
        for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
            oligo->axons[i].myelination_level *= scale;
            oligo->myelination_levels[i] = oligo->axons[i].myelination_level;
        }
    }

    // Update statistics
    if (oligo->num_myelinated_axons > 0) {
        oligo->avg_conduction_velocity = total_velocity / (float)oligo->num_myelinated_axons;
    }

    oligo->last_remodeling_time = nimcp_time_monotonic_us();

    nimcp_spinlock_unlock(&oligo->lock);
}

void oligodendrocyte_set_axon_centrality(oligodendrocyte_t* oligo,
                                          uint32_t axon_id,
                                          float centrality) {
    if (!oligo) return;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    if (idx >= 0) {
        oligo->axons[idx].centrality_score = nimcp_clampf(centrality, 0.0F, 1.0F);
        oligo->axons[idx].priority_myelination =
            (centrality >= NIMCP_OLIGO_CENTRALITY_MIN_PRIORITY);
    }

    nimcp_spinlock_unlock(&oligo->lock);
}

//=============================================================================
// NRG1/BDNF GROWTH FACTOR SIGNALING
//=============================================================================

void oligodendrocyte_update_growth_factors(oligodendrocyte_t* oligo, float dt) {
    if (!oligo || dt <= 0.0F) return;

    nimcp_spinlock_lock(&oligo->lock);

    // Compute total activity from axons
    float total_activity = 0.0F;
    if (oligo->num_myelinated_axons > 0) {
        for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
            total_activity += oligo->axons[i].activity_score;
        }
        total_activity /= oligo->num_myelinated_axons;
    }

    // Update each growth factor
    for (int i = 0; i < NIMCP_GROWTH_FACTOR_COUNT; i++) {
        // Production based on activity and maturation
        float production = oligo->growth_factors.production_rates[i];

        // Activity-dependent production for BDNF
        if (i == GROWTH_FACTOR_BDNF) {
            production += total_activity * 0.1F;
        }

        // Decay
        float decay = NIMCP_GROWTH_FACTOR_DECAY_RATE * oligo->growth_factors.concentrations[i];

        // Update concentration
        oligo->growth_factors.concentrations[i] += dt * (production - decay);
        oligo->growth_factors.concentrations[i] = nimcp_clampf(
            oligo->growth_factors.concentrations[i],
            0.0F,
            NIMCP_GROWTH_FACTOR_MAX_CONCENTRATION);
    }

    oligo->growth_factors.last_update_time = nimcp_time_monotonic_us();

    nimcp_spinlock_unlock(&oligo->lock);
}

float oligodendrocyte_get_growth_factor(const oligodendrocyte_t* oligo,
                                         growth_factor_type_t type) {
    if (!oligo || type >= NIMCP_GROWTH_FACTOR_COUNT) return 0.0F;
    return oligo->growth_factors.concentrations[type];
}

void oligodendrocyte_add_growth_factor(oligodendrocyte_t* oligo,
                                        growth_factor_type_t type,
                                        float amount) {
    if (!oligo || type >= NIMCP_GROWTH_FACTOR_COUNT) return;

    nimcp_spinlock_lock(&oligo->lock);

    oligo->growth_factors.concentrations[type] += amount;
    oligo->growth_factors.concentrations[type] = nimcp_clampf(
        oligo->growth_factors.concentrations[type],
        0.0F,
        NIMCP_GROWTH_FACTOR_MAX_CONCENTRATION);

    nimcp_spinlock_unlock(&oligo->lock);
}

float oligodendrocyte_compute_myelin_signal(const oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo) return 0.0F;

    float nrg1 = oligo->growth_factors.concentrations[GROWTH_FACTOR_NRG1];
    float bdnf = oligo->growth_factors.concentrations[GROWTH_FACTOR_BDNF];
    float igf1 = oligo->growth_factors.concentrations[GROWTH_FACTOR_IGF1];

    // Get axon sensitivity
    float nrg1_sens = 1.0F;
    float bdnf_sens = 1.0F;

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        if (oligo->axons[i].axon_id == axon_id) {
            nrg1_sens = oligo->axons[i].nrg1_sensitivity;
            bdnf_sens = oligo->axons[i].bdnf_sensitivity;
            break;
        }
    }

    float signal = nrg1 * NIMCP_NRG1_MYELIN_COEFFICIENT * nrg1_sens +
                   bdnf * NIMCP_BDNF_MYELIN_COEFFICIENT * bdnf_sens +
                   igf1 * 0.2F;

    return nimcp_clampf(signal / NIMCP_GROWTH_FACTOR_MAX_CONCENTRATION, 0.0F, 1.0F);
}

//=============================================================================
// LACTATE SHUTTLE (METABOLIC SUPPORT)
//=============================================================================

void oligodendrocyte_update_lactate_shuttle(oligodendrocyte_t* oligo, float dt) {
    if (!oligo || dt <= 0.0F) return;

    nimcp_spinlock_lock(&oligo->lock);

    // Produce lactate from glucose
    float production = oligo->lactate_shuttle.glucose_uptake * oligo->glucose_level *
                       NIMCP_OLIGO_LACTATE_MAX_PRODUCTION * dt;
    oligo->lactate_shuttle.lactate_pool += production;

    // Cap lactate pool
    oligo->lactate_shuttle.lactate_pool = nimcp_clampf(oligo->lactate_shuttle.lactate_pool, 0.0F, 2.0F);

    // Distribute lactate to myelinated axons
    if (oligo->num_myelinated_axons > 0 && oligo->lactate_shuttle.lactate_pool > 0.0F) {
        // Calculate total demand
        float total_demand = 0.0F;
        for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
            total_demand += oligo->axons[i].metabolic_demand;
        }

        if (total_demand > 0.0F) {
            // Distribute proportionally to demand
            float available = oligo->lactate_shuttle.lactate_pool *
                              NIMCP_OLIGO_LACTATE_TRANSFER_EFFICIENCY * dt;
            available = fminf(available, oligo->lactate_shuttle.lactate_pool);

            float total_delivered = 0.0F;
            for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
                float fraction = oligo->axons[i].metabolic_demand / total_demand;
                float delivery = available * fraction;

                oligo->axons[i].lactate_received = delivery;
                oligo->lactate_shuttle.axon_lactate_delivery[i] = delivery;
                total_delivered += delivery;
            }

            oligo->lactate_shuttle.lactate_pool -= total_delivered;
            oligo->lactate_shuttle.transfer_rate = total_delivered / dt;
            oligo->total_lactate_delivered += total_delivered;
        }
    }

    // Apply decay
    oligo->lactate_shuttle.lactate_pool *= (1.0F - NIMCP_OLIGO_LACTATE_DECAY_RATE * dt);

    // Update supported axon count
    uint32_t supported = 0;
    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        if (oligo->axons[i].lactate_received >= NIMCP_OLIGO_LACTATE_CRITICAL) {
            supported++;
        }
    }
    oligo->lactate_shuttle.supported_axon_count = supported;

    nimcp_spinlock_unlock(&oligo->lock);
}

float oligodendrocyte_get_axon_lactate(const oligodendrocyte_t* oligo, uint32_t axon_id) {
    if (!oligo) return 0.0F;

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        if (oligo->axons[i].axon_id == axon_id) {
            return oligo->axons[i].lactate_received;
        }
    }
    return 0.0F;
}

void oligodendrocyte_set_axon_demand(oligodendrocyte_t* oligo,
                                      uint32_t axon_id,
                                      float demand) {
    if (!oligo) return;

    nimcp_spinlock_lock(&oligo->lock);

    int32_t idx = find_axon_index(oligo, axon_id);
    if (idx >= 0) {
        oligo->axons[idx].metabolic_demand = nimcp_clampf(demand, 0.0F, 10.0F);
    }

    nimcp_spinlock_unlock(&oligo->lock);
}

bool oligodendrocyte_axon_metabolically_supported(const oligodendrocyte_t* oligo,
                                                   uint32_t axon_id) {
    if (!oligo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligodendrocyte_get_axon_lactate: oligo is NULL");
        return false;
    }

    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        if (oligo->axons[i].axon_id == axon_id) {
            return oligo->axons[i].lactate_received >= NIMCP_OLIGO_LACTATE_CRITICAL;
        }
    }
    return false;
}

//=============================================================================
// STATE DYNAMICS (RK4 ODE)
//=============================================================================

void oligodendrocyte_update_state_dynamics(oligodendrocyte_t* oligo, float dt) {
    if (!oligo || dt <= 0.0F) return;

    // Process pending bio-async messages before state update
    if (g_oligo_bio_ctx) {
        bio_router_process_inbox(g_oligo_bio_ctx, 5);  // Process up to 5 messages
    }

    nimcp_spinlock_lock(&oligo->lock);

    rk4_step(oligo, dt);

    // Check for maturation transition
    if (oligo->maturation_progress >= 1.0F && oligo->maturation < OLIGO_STATE_MATURE) {
        oligo->maturation = (oligo_maturation_state_t)(oligo->maturation + 1);
        oligo->maturation_progress = 0.0F;
        oligo->maturation_time = nimcp_time_monotonic_us();
    }

    nimcp_spinlock_unlock(&oligo->lock);

    // Publish myelination state via SEROTONIN channel (slow, stabilizing)
    if (g_oligo_bio_ctx) {
        bio_router_publish_signal(g_oligo_bio_ctx, "oligodendrocyte.myelination", oligo->myelination_rate);
        bio_router_publish_signal(g_oligo_bio_ctx, "oligodendrocyte.maturation", oligo->maturation_progress);
        LOG_MODULE_DEBUG("OLIGODENDROCYTE", "Published state: myelin_rate=%.3f, maturation=%.3f",
                         oligo->myelination_rate, oligo->maturation_progress);
    }
}

oligo_maturation_state_t oligodendrocyte_get_maturation(const oligodendrocyte_t* oligo) {
    if (!oligo) return OLIGO_STATE_OPC;
    return oligo->maturation;
}

bool oligodendrocyte_advance_maturation(oligodendrocyte_t* oligo) {
    if (!oligo || oligo->maturation >= OLIGO_STATE_MATURE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "oligodendrocyte_advance_maturation: oligo is NULL");
        return false;
    }

    nimcp_spinlock_lock(&oligo->lock);

    oligo->maturation = (oligo_maturation_state_t)(oligo->maturation + 1);
    oligo->maturation_progress = 0.0F;
    oligo->maturation_time = nimcp_time_monotonic_us();

    nimcp_spinlock_unlock(&oligo->lock);
    return true;
}

//=============================================================================
// METABOLIC MANAGEMENT
//=============================================================================

void oligodendrocyte_update_atp(oligodendrocyte_t* oligo, float dt) {
    if (!oligo || dt <= 0.0F) return;

    nimcp_spinlock_lock(&oligo->lock);

    // ATP cost proportional to total myelination
    float total_myelin = 0.0F;
    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        total_myelin += oligo->axons[i].myelination_level;
    }

    float cost = total_myelin * NIMCP_OLIGO_ATP_COST_PER_MYELIN +
                 oligo->lactate_shuttle.production_rate * 0.01F;
    oligo->metabolic_cost = cost;

    // Regeneration from glucose
    float regen = NIMCP_OLIGO_ATP_REGEN_RATE + oligo->glucose_level * NIMCP_OLIGO_GLUCOSE_UPTAKE_RATE;

    // Update ATP
    oligo->atp_level += dt * (regen - cost);
    oligo->atp_level = nimcp_clampf(oligo->atp_level, 0.0F, NIMCP_OLIGO_ATP_MAX);

    // Sync with state variable
    oligo->state_variables[2] = oligo->atp_level;

    nimcp_spinlock_unlock(&oligo->lock);
}

float oligodendrocyte_get_atp_level(const oligodendrocyte_t* oligo) {
    if (!oligo) return 0.0F;
    return oligo->atp_level;
}

void oligodendrocyte_add_glucose(oligodendrocyte_t* oligo, float amount) {
    if (!oligo) return;

    nimcp_spinlock_lock(&oligo->lock);
    oligo->glucose_level += amount;
    oligo->glucose_level = nimcp_clampf(oligo->glucose_level, 0.0F, 1.0F);
    nimcp_spinlock_unlock(&oligo->lock);
}

//=============================================================================
// NETWORK OPERATIONS
//=============================================================================

nimcp_result_t oligodendrocyte_network_add(oligodendrocyte_network_t* network,
                                            oligodendrocyte_t* oligo) {
    NIMCP_CHECK_THROW(network && oligo, NIMCP_ERROR_INVALID_PARAM, "network or oligo is NULL");

    nimcp_mutex_lock(&network->lock);

    if (network->num_oligodendrocytes >= network->capacity) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    network->oligodendrocytes[network->num_oligodendrocytes] = oligo;
    network->num_oligodendrocytes++;
    network->spatial_index_valid = false;

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

void oligodendrocyte_network_rebuild_spatial_index(oligodendrocyte_network_t* network) {
    if (!network) return;

    nimcp_mutex_lock(&network->lock);

    // Destroy existing trees
    if (network->oligo_tree) {
        kdtree_destroy(network->oligo_tree);
        network->oligo_tree = NULL;
    }

    // Create new KD-tree for oligodendrocytes
    network->oligo_tree = kdtree_create();

    if (network->oligo_tree && network->num_oligodendrocytes > 0) {
        kdtree_point_t* points = (kdtree_point_t*)nimcp_malloc(
            network->num_oligodendrocytes * sizeof(kdtree_point_t));
        void** user_data = (void**)nimcp_malloc(
            network->num_oligodendrocytes * sizeof(void*));

        if (points && user_data) {
            for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
                oligodendrocyte_t* oligo = network->oligodendrocytes[i];
                if (oligo) {
                    points[i][0] = oligo->position[0];
                    points[i][1] = oligo->position[1];
                    points[i][2] = oligo->position[2];
                    user_data[i] = oligo;
                }
            }

            kdtree_build(network->oligo_tree, points, user_data, network->num_oligodendrocytes);
        }

        if (points) nimcp_free(points);
        if (user_data) nimcp_free(user_data);
    }

    network->spatial_index_valid = true;

    nimcp_mutex_unlock(&network->lock);
}

void oligodendrocyte_network_update_centrality(oligodendrocyte_network_t* network,
                                                 void* axon_graph) {
    if (!network) return;
    // Centrality computation would integrate with nimcp_centrality.h
    // For now, mark as valid
    network->centrality_valid = true;
}

void oligodendrocyte_network_step(oligodendrocyte_network_t* network, float dt) {
    if (!network || dt <= 0.0F) return;

    // Skip-frame: myelination remodeling is biologically slow (hours-days)
    // Update every 100th step instead of every step
    static _Thread_local uint32_t oligo_step_counter = 0;
    oligo_step_counter++;
    if (oligo_step_counter % 100 != 0) return;

    nimcp_mutex_lock(&network->lock);

    // Use accumulated dt (100 steps × dt) for accurate dynamics
    float accumulated_dt = dt * 100.0F;

    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        if (!oligo) continue;

        // 1. Update state dynamics (RK4)
        oligodendrocyte_update_state_dynamics(oligo, accumulated_dt);

        // 2. Update growth factors
        oligodendrocyte_update_growth_factors(oligo, accumulated_dt);

        // 3. Remodel myelination
        oligodendrocyte_remodel_myelination(oligo, accumulated_dt);

        // 4. Optimize G-ratios
        oligodendrocyte_optimize_g_ratios(oligo, accumulated_dt);

        // 5. Update lactate shuttle
        oligodendrocyte_update_lactate_shuttle(oligo, accumulated_dt);

        // 6. Update ATP
        oligodendrocyte_update_atp(oligo, accumulated_dt);
    }

    // 7. Diffuse growth factors between nearby oligodendrocytes
    oligodendrocyte_network_diffuse_growth_factors(network, accumulated_dt);

    nimcp_mutex_unlock(&network->lock);
}

oligodendrocyte_t* oligodendrocyte_network_find_by_axon(oligodendrocyte_network_t* network,
                                                         uint32_t axon_id) {
    return oligodendrocyte_network_find_by_neuron(network, axon_id);
}

oligodendrocyte_t* oligodendrocyte_network_find_by_neuron(oligodendrocyte_network_t* network,
                                                           uint32_t neuron_id) {
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    nimcp_mutex_lock(&network->lock);

    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        if (!oligo) continue;

        int32_t idx = find_axon_index(oligo, neuron_id);
        if (idx >= 0) {
            nimcp_mutex_unlock(&network->lock);
            return oligo;
        }
    }

    nimcp_mutex_unlock(&network->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligodendrocyte_network_step: capacity exceeded");
    return NULL;
}

oligodendrocyte_t* oligodendrocyte_network_find_nearest(oligodendrocyte_network_t* network,
                                                         float x, float y, float z) {
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    nimcp_mutex_lock(&network->lock);

    // Use KD-tree if available
    if (network->oligo_tree && network->spatial_index_valid) {
        kdtree_point_t query;
        query[0] = x;
        query[1] = y;
        query[2] = z;
        float dist_sq = 0.0F;
        void* nearest = kdtree_nearest(network->oligo_tree, query, &dist_sq);
        if (nearest) {
            nimcp_mutex_unlock(&network->lock);
            return (oligodendrocyte_t*)nearest;
        }
    }

    // Fallback to linear search
    oligodendrocyte_t* nearest = NULL;
    float min_dist_sq = INFINITY;

    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        if (!oligo) continue;

        float dx = oligo->position[0] - x;
        float dy = oligo->position[1] - y;
        float dz = oligo->position[2] - z;
        float dist_sq = dx*dx + dy*dy + dz*dz;

        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            nearest = oligo;
        }
    }

    nimcp_mutex_unlock(&network->lock);
    return nearest;
}

uint32_t oligodendrocyte_network_find_in_radius(oligodendrocyte_network_t* network,
                                                  float x, float y, float z,
                                                  float radius,
                                                  oligodendrocyte_t** results,
                                                  uint32_t max_results) {
    if (!network || !results || max_results == 0) return 0;

    nimcp_mutex_lock(&network->lock);

    uint32_t count = 0;
    float radius_sq = radius * radius;

    for (uint32_t i = 0; i < network->num_oligodendrocytes && count < max_results; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        if (!oligo) continue;

        float dx = oligo->position[0] - x;
        float dy = oligo->position[1] - y;
        float dz = oligo->position[2] - z;
        float dist_sq = dx*dx + dy*dy + dz*dz;

        if (dist_sq <= radius_sq) {
            results[count++] = oligo;
        }
    }

    nimcp_mutex_unlock(&network->lock);
    return count;
}

void oligodendrocyte_network_diffuse_growth_factors(oligodendrocyte_network_t* network,
                                                      float dt) {
    if (!network || network->num_oligodendrocytes < 2) return;

    // Simple diffusion between nearby oligodendrocytes
    float diffusion_radius = 100.0F; // µm
    float diffusion_rate = NIMCP_GROWTH_FACTOR_DIFFUSION_COEFF * dt / (diffusion_radius * diffusion_radius);

    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo1 = network->oligodendrocytes[i];
        if (!oligo1) continue;

        for (uint32_t j = i + 1; j < network->num_oligodendrocytes; j++) {
            oligodendrocyte_t* oligo2 = network->oligodendrocytes[j];
            if (!oligo2) continue;

            // Check distance
            float dx = oligo1->position[0] - oligo2->position[0];
            float dy = oligo1->position[1] - oligo2->position[1];
            float dz = oligo1->position[2] - oligo2->position[2];
            float dist_sq = dx*dx + dy*dy + dz*dz;

            if (dist_sq < diffusion_radius * diffusion_radius) {
                float dist_factor = 1.0F - sqrtf(dist_sq) / diffusion_radius;

                // DEADLOCK FIX: Acquire locks in consistent ID order (lower ID first)
                // to prevent deadlock when called from multiple threads
                oligodendrocyte_t* first = (oligo1->id < oligo2->id) ? oligo1 : oligo2;
                oligodendrocyte_t* second = (oligo1->id < oligo2->id) ? oligo2 : oligo1;

                nimcp_spinlock_lock(&first->lock);
                nimcp_spinlock_lock(&second->lock);

                // Exchange growth factors while holding both locks
                for (int k = 0; k < NIMCP_GROWTH_FACTOR_COUNT; k++) {
                    float diff = oligo1->growth_factors.concentrations[k] -
                                 oligo2->growth_factors.concentrations[k];
                    float exchange = diff * diffusion_rate * dist_factor;

                    oligo1->growth_factors.concentrations[k] -= exchange * 0.5F;
                    oligo2->growth_factors.concentrations[k] += exchange * 0.5F;
                }

                nimcp_spinlock_unlock(&second->lock);
                nimcp_spinlock_unlock(&first->lock);
            }
        }
    }
}

void oligodendrocyte_network_get_stats(const oligodendrocyte_network_t* network,
                                         oligodendrocyte_network_stats_t* stats) {
    if (!network || !stats) return;

    memset(stats, 0, sizeof(oligodendrocyte_network_stats_t));

    stats->total_oligodendrocytes = network->num_oligodendrocytes;
    stats->min_conduction_velocity = INFINITY;
    stats->max_conduction_velocity = 0.0F;

    float total_myelin = 0.0F;
    float total_g_ratio = 0.0F;
    float total_velocity = 0.0F;
    uint32_t myelinated_count = 0;

    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        if (!oligo) continue;

        // Count by maturation state
        switch (oligo->maturation) {
            case OLIGO_STATE_OPC: stats->opc_count++; break;
            case OLIGO_STATE_PRE_OL: stats->pre_ol_count++; break;
            case OLIGO_STATE_IMMATURE: stats->immature_count++; break;
            case OLIGO_STATE_MATURE: stats->mature_count++; break;
        }

        stats->total_myelinated_axons += oligo->num_myelinated_axons;
        stats->total_nrg1 += oligo->growth_factors.concentrations[GROWTH_FACTOR_NRG1];
        stats->total_bdnf += oligo->growth_factors.concentrations[GROWTH_FACTOR_BDNF];
        stats->total_lactate_delivered += oligo->total_lactate_delivered;

        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            myelinated_axon_t* axon = &oligo->axons[j];

            stats->total_internode_segments += axon->num_internodes;
            total_myelin += axon->myelination_level;

            if (axon->myelination_level > 0.0F) {
                total_g_ratio += axon->g_ratio;
                total_velocity += axon->conduction_velocity;
                myelinated_count++;

                if (axon->conduction_velocity < stats->min_conduction_velocity) {
                    stats->min_conduction_velocity = axon->conduction_velocity;
                }
                if (axon->conduction_velocity > stats->max_conduction_velocity) {
                    stats->max_conduction_velocity = axon->conduction_velocity;
                }
            }
        }
    }

    if (stats->total_myelinated_axons > 0) {
        stats->avg_myelination_level = total_myelin / (float)stats->total_myelinated_axons;
    }
    if (myelinated_count > 0) {
        stats->avg_g_ratio = total_g_ratio / (float)myelinated_count;
        stats->avg_conduction_velocity = total_velocity / (float)myelinated_count;
    }
    if (stats->min_conduction_velocity == INFINITY) {
        stats->min_conduction_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS;
    }

    // Compute network efficiency
    float optimal_velocity = NIMCP_OLIGO_BASE_VELOCITY_MS * NIMCP_OLIGO_MYELIN_MULTIPLIER;
    stats->network_myelination_efficiency =
        (stats->avg_conduction_velocity / optimal_velocity) * stats->avg_myelination_level;
}

//=============================================================================
// MEMORY POOL OPERATIONS (Phase 1.5+)
//=============================================================================

oligo_axon_pool_t* oligo_axon_pool_create(uint32_t capacity) {
    if (capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligo_axon_pool_create: capacity is zero");
        return NULL;
    }

    // Round up to nearest 64 for bitmap alignment
    uint32_t aligned_capacity = ((capacity + 63) / 64) * 64;
    uint32_t num_bitmap_words = aligned_capacity / 64;

    oligo_axon_pool_t* pool = (oligo_axon_pool_t*)nimcp_malloc(sizeof(oligo_axon_pool_t));
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    pool->buffer = (myelinated_axon_t*)nimcp_malloc(aligned_capacity * sizeof(myelinated_axon_t));
    if (!pool->buffer) {
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligo_axon_pool_create: pool->buffer is NULL");
        return NULL;
    }

    pool->bitmap = (uint64_t*)nimcp_malloc(num_bitmap_words * sizeof(uint64_t));
    if (!pool->bitmap) {
        nimcp_free(pool->buffer);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligo_axon_pool_create: pool->bitmap is NULL");
        return NULL;
    }

    // Initialize: all bits set to 1 (free)
    for (uint32_t i = 0; i < num_bitmap_words; i++) {
        pool->bitmap[i] = UINT64_MAX;
    }

    pool->capacity = aligned_capacity;
    pool->num_bitmap_words = num_bitmap_words;
    pool->allocated_count = 0;
    nimcp_spinlock_init(&pool->lock);

    memset(pool->buffer, 0, aligned_capacity * sizeof(myelinated_axon_t));

    return pool;
}

void oligo_axon_pool_destroy(oligo_axon_pool_t* pool) {
    if (!pool) return;

    if (pool->buffer) nimcp_free(pool->buffer);
    if (pool->bitmap) nimcp_free(pool->bitmap);
    nimcp_free(pool);
}

myelinated_axon_t* oligo_axon_pool_alloc(oligo_axon_pool_t* pool) {
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&pool->lock);

    // Find first word with free bit (O(1) amortized)
    for (uint32_t w = 0; w < pool->num_bitmap_words; w++) {
        if (pool->bitmap[w] != 0) {
            // Find first set bit using __builtin_ctzll
            int bit = __builtin_ctzll(pool->bitmap[w]);
            uint32_t index = w * 64 + bit;

            if (index < pool->capacity) {
                // Clear bit (mark as allocated)
                pool->bitmap[w] &= ~(1ULL << bit);
                pool->allocated_count++;

                nimcp_spinlock_unlock(&pool->lock);

                // Zero initialize the slot
                myelinated_axon_t* axon = &pool->buffer[index];
                memset(axon, 0, sizeof(myelinated_axon_t));
                return axon;
            }
        }
    }

    nimcp_spinlock_unlock(&pool->lock);
    return NULL; // Pool exhausted
}

void oligo_axon_pool_free(oligo_axon_pool_t* pool, myelinated_axon_t* axon) {
    if (!pool || !axon) return;

    // Verify pointer is within pool bounds
    if (axon < pool->buffer || axon >= pool->buffer + pool->capacity) {
        return; // Not from this pool
    }

    nimcp_spinlock_lock(&pool->lock);

    uint32_t index = (uint32_t)(axon - pool->buffer);
    uint32_t word = index / 64;
    uint32_t bit = index % 64;

    // Set bit (mark as free)
    pool->bitmap[word] |= (1ULL << bit);
    if (pool->allocated_count > 0) {
        pool->allocated_count--;
    }

    nimcp_spinlock_unlock(&pool->lock);
}

void oligo_axon_pool_stats(const oligo_axon_pool_t* pool,
                           uint32_t* allocated, uint32_t* capacity) {
    if (!pool) {
        if (allocated) *allocated = 0;
        if (capacity) *capacity = 0;
        return;
    }

    if (allocated) *allocated = pool->allocated_count;
    if (capacity) *capacity = pool->capacity;
}

oligo_internode_pool_t* oligo_internode_pool_create(uint32_t capacity) {
    if (capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligo_internode_pool_create: capacity is zero");
        return NULL;
    }

    // Round up to nearest 64 for bitmap alignment
    uint32_t aligned_capacity = ((capacity + 63) / 64) * 64;
    uint32_t num_bitmap_words = aligned_capacity / 64;

    oligo_internode_pool_t* pool = (oligo_internode_pool_t*)nimcp_malloc(sizeof(oligo_internode_pool_t));
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    pool->buffer = (internode_segment_t*)nimcp_malloc(aligned_capacity * sizeof(internode_segment_t));
    if (!pool->buffer) {
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligo_internode_pool_create: pool->buffer is NULL");
        return NULL;
    }

    pool->bitmap = (uint64_t*)nimcp_malloc(num_bitmap_words * sizeof(uint64_t));
    if (!pool->bitmap) {
        nimcp_free(pool->buffer);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligo_internode_pool_create: pool->bitmap is NULL");
        return NULL;
    }

    // Initialize: all bits set to 1 (free)
    for (uint32_t i = 0; i < num_bitmap_words; i++) {
        pool->bitmap[i] = UINT64_MAX;
    }

    pool->capacity = aligned_capacity;
    pool->num_bitmap_words = num_bitmap_words;
    pool->allocated_count = 0;
    nimcp_spinlock_init(&pool->lock);

    memset(pool->buffer, 0, aligned_capacity * sizeof(internode_segment_t));

    return pool;
}

void oligo_internode_pool_destroy(oligo_internode_pool_t* pool) {
    if (!pool) return;

    if (pool->buffer) nimcp_free(pool->buffer);
    if (pool->bitmap) nimcp_free(pool->bitmap);
    nimcp_free(pool);
}

internode_segment_t* oligo_internode_pool_alloc(oligo_internode_pool_t* pool) {
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&pool->lock);

    // Find first word with free bit (O(1) amortized)
    for (uint32_t w = 0; w < pool->num_bitmap_words; w++) {
        if (pool->bitmap[w] != 0) {
            // Find first set bit using __builtin_ctzll
            int bit = __builtin_ctzll(pool->bitmap[w]);
            uint32_t index = w * 64 + bit;

            if (index < pool->capacity) {
                // Clear bit (mark as allocated)
                pool->bitmap[w] &= ~(1ULL << bit);
                pool->allocated_count++;

                nimcp_spinlock_unlock(&pool->lock);

                // Zero initialize the slot
                internode_segment_t* internode = &pool->buffer[index];
                memset(internode, 0, sizeof(internode_segment_t));
                return internode;
            }
        }
    }

    nimcp_spinlock_unlock(&pool->lock);
    return NULL; // Pool exhausted
}

void oligo_internode_pool_free(oligo_internode_pool_t* pool, internode_segment_t* internode) {
    if (!pool || !internode) return;

    // Verify pointer is within pool bounds
    if (internode < pool->buffer || internode >= pool->buffer + pool->capacity) {
        return; // Not from this pool
    }

    nimcp_spinlock_lock(&pool->lock);

    uint32_t index = (uint32_t)(internode - pool->buffer);
    uint32_t word = index / 64;
    uint32_t bit = index % 64;

    // Set bit (mark as free)
    pool->bitmap[word] |= (1ULL << bit);
    if (pool->allocated_count > 0) {
        pool->allocated_count--;
    }

    nimcp_spinlock_unlock(&pool->lock);
}

void oligo_internode_pool_stats(const oligo_internode_pool_t* pool,
                                uint32_t* allocated, uint32_t* capacity) {
    if (!pool) {
        if (allocated) *allocated = 0;
        if (capacity) *capacity = 0;
        return;
    }

    if (allocated) *allocated = pool->allocated_count;
    if (capacity) *capacity = pool->capacity;
}

//=============================================================================
// COPY-ON-WRITE OPERATIONS (Phase 1.5+)
//=============================================================================

oligodendrocyte_t* oligodendrocyte_cow_copy(oligodendrocyte_t* oligo) {
    if (!oligo) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligo is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&oligo->lock);

    // Increment reference count on original
    oligo->cow_ref_count++;

    // Create shallow copy structure
    oligodendrocyte_t* copy = (oligodendrocyte_t*)nimcp_malloc(sizeof(oligodendrocyte_t));
    if (!copy) {
        oligo->cow_ref_count--;
        nimcp_spinlock_unlock(&oligo->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligodendrocyte_cow_copy: copy is NULL");
        return NULL;
    }

    // Copy all fields (shallow copy)
    memcpy(copy, oligo, sizeof(oligodendrocyte_t));

    // Set up CoW tracking
    copy->cow_ref_count = 1;
    copy->cow_modified = false;
    copy->cow_original = oligo;

    // Initialize lock for the copy
    nimcp_spinlock_init(&copy->lock);

    nimcp_spinlock_unlock(&oligo->lock);

    return copy;
}

nimcp_result_t oligodendrocyte_cow_prepare_write(oligodendrocyte_t* oligo) {
    NIMCP_CHECK_THROW(oligo, NIMCP_ERROR_INVALID_PARAM, "oligo is NULL");

    nimcp_spinlock_lock(&oligo->lock);

    // If already modified or not a CoW copy, nothing to do
    if (oligo->cow_modified || oligo->cow_original == NULL) {
        oligo->cow_modified = true;
        nimcp_spinlock_unlock(&oligo->lock);
        return NIMCP_SUCCESS;
    }

    // Need to deep copy the shared arrays
    oligodendrocyte_t* original = (oligodendrocyte_t*)oligo->cow_original;

    // Deep copy enhanced axon array
    if (original->axons && original->max_axons > 0) {
        oligo->axons = (myelinated_axon_t*)nimcp_malloc(original->max_axons * sizeof(myelinated_axon_t));
        if (!oligo->axons) {
            nimcp_spinlock_unlock(&oligo->lock);
            return NIMCP_ERROR_MEMORY;
        }
        memcpy(oligo->axons, original->axons, original->max_axons * sizeof(myelinated_axon_t));

        // Deep copy internodes for each axon
        for (uint32_t i = 0; i < original->max_axons; i++) {
            if (original->axons[i].internodes && original->axons[i].max_internodes > 0) {
                oligo->axons[i].internodes = (internode_segment_t*)nimcp_malloc(
                    original->axons[i].max_internodes * sizeof(internode_segment_t));
                if (!oligo->axons[i].internodes) {
                    // Rollback on failure
                    for (uint32_t j = 0; j < i; j++) {
                        if (oligo->axons[j].internodes) {
                            nimcp_free(oligo->axons[j].internodes);
                        }
                    }
                    nimcp_free(oligo->axons);
                    oligo->axons = original->axons;
                    nimcp_spinlock_unlock(&oligo->lock);
                    return NIMCP_ERROR_MEMORY;
                }
                memcpy(oligo->axons[i].internodes, original->axons[i].internodes,
                       original->axons[i].max_internodes * sizeof(internode_segment_t));
            }
        }
    }

    // Deep copy legacy arrays
    if (original->myelinated_neuron_ids && original->max_axons > 0) {
        oligo->myelinated_neuron_ids = (uint32_t*)nimcp_malloc(original->max_axons * sizeof(uint32_t));
        if (oligo->myelinated_neuron_ids) {
            memcpy(oligo->myelinated_neuron_ids, original->myelinated_neuron_ids,
                   original->max_axons * sizeof(uint32_t));
        }
    }

    if (original->myelination_levels && original->max_axons > 0) {
        oligo->myelination_levels = (float*)nimcp_malloc(original->max_axons * sizeof(float));
        if (oligo->myelination_levels) {
            memcpy(oligo->myelination_levels, original->myelination_levels,
                   original->max_axons * sizeof(float));
        }
    }

    if (original->neuron_activity_history && original->max_axons > 0) {
        oligo->neuron_activity_history = (float*)nimcp_malloc(original->max_axons * sizeof(float));
        if (oligo->neuron_activity_history) {
            memcpy(oligo->neuron_activity_history, original->neuron_activity_history,
                   original->max_axons * sizeof(float));
        }
    }

    if (original->last_spike_times && original->max_axons > 0) {
        oligo->last_spike_times = (uint64_t*)nimcp_malloc(original->max_axons * sizeof(uint64_t));
        if (oligo->last_spike_times) {
            memcpy(oligo->last_spike_times, original->last_spike_times,
                   original->max_axons * sizeof(uint64_t));
        }
    }

    // Deep copy lactate delivery array
    if (original->lactate_shuttle.axon_lactate_delivery && original->max_axons > 0) {
        oligo->lactate_shuttle.axon_lactate_delivery = (float*)nimcp_malloc(
            original->max_axons * sizeof(float));
        if (oligo->lactate_shuttle.axon_lactate_delivery) {
            memcpy(oligo->lactate_shuttle.axon_lactate_delivery,
                   original->lactate_shuttle.axon_lactate_delivery,
                   original->max_axons * sizeof(float));
        }
    }

    oligo->cow_modified = true;

    nimcp_spinlock_unlock(&oligo->lock);
    return NIMCP_SUCCESS;
}

void oligodendrocyte_cow_release(oligodendrocyte_t* oligo) {
    if (!oligo) return;

    nimcp_spinlock_lock(&oligo->lock);

    // Decrement reference count
    if (oligo->cow_ref_count > 0) {
        oligo->cow_ref_count--;
    }

    // If this is a CoW copy that was modified, decrement original's ref count
    if (oligo->cow_original != NULL) {
        oligodendrocyte_t* original = (oligodendrocyte_t*)oligo->cow_original;
        nimcp_spinlock_lock(&original->lock);
        if (original->cow_ref_count > 0) {
            original->cow_ref_count--;
        }
        nimcp_spinlock_unlock(&original->lock);
    }

    nimcp_spinlock_unlock(&oligo->lock);
}

bool oligodendrocyte_is_cow_copy(const oligodendrocyte_t* oligo) {
    if (!oligo) {
        return false;
    }
    return oligo->cow_original != NULL;
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

float oligodendrocyte_get_total_myelination(oligodendrocyte_t* oligo) {
    if (!oligo) return 0.0F;

    float total = 0.0F;
    for (uint32_t i = 0; i < oligo->num_myelinated_axons; i++) {
        total += oligo->axons[i].myelination_level;
    }
    return total;
}

float oligodendrocyte_get_avg_conduction_velocity(oligodendrocyte_t* oligo) {
    if (!oligo || oligo->num_myelinated_axons == 0) return NIMCP_OLIGO_BASE_VELOCITY_MS;
    return oligo->avg_conduction_velocity;
}

const char* oligo_maturation_state_to_string(oligo_maturation_state_t state) {
    switch (state) {
        case OLIGO_STATE_OPC: return "OPC";
        case OLIGO_STATE_PRE_OL: return "Pre-OL";
        case OLIGO_STATE_IMMATURE: return "Immature";
        case OLIGO_STATE_MATURE: return "Mature";
        default: return "Unknown";
    }
}

const char* myelin_state_to_string(myelin_state_t state) {
    switch (state) {
        case MYELIN_STATE_UNMYELINATED: return "Unmyelinated";
        case MYELIN_STATE_INITIATING: return "Initiating";
        case MYELIN_STATE_PARTIAL: return "Partial";
        case MYELIN_STATE_MATURE: return "Mature";
        case MYELIN_STATE_DEGENERATING: return "Degenerating";
        default: return "Unknown";
    }
}

const char* growth_factor_type_to_string(growth_factor_type_t type) {
    switch (type) {
        case GROWTH_FACTOR_NRG1: return "NRG1";
        case GROWTH_FACTOR_BDNF: return "BDNF";
        case GROWTH_FACTOR_IGF1: return "IGF-1";
        case GROWTH_FACTOR_NT3: return "NT-3";
        default: return "Unknown";
    }
}
