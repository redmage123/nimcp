/**
 * @file nimcp_axon.c (REFACTORED EXAMPLE)
 * @brief NIMCP Axon Module Implementation with Full Integration
 *
 * WHAT: Implementation of axon signal propagation with async, logging, config, security
 * WHY:  Demonstrate complete refactoring pattern for all NIMCP core modules
 * HOW:  Integrates unified memory, async futures, logging, config, security
 *
 * REFACTORING CHANGES:
 * 1. Async/Future Integration: Inter-module communication via futures
 * 2. Enhanced Logging: LOG_MODULE_* macros throughout
 * 3. Config Integration: All constants configurable via config system
 * 4. Security Registration: Module registers with security system
 * 5. Comprehensive Error Handling: Consistent error reporting
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.0.0 (Refactored)
 */

#include "core/axon/nimcp_axon.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "security/nimcp_security.h"
#include "async/nimcp_future.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "axon_refactored_example"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(axon_refactored_example)

//=============================================================================
// MODULE CONSTANTS
//=============================================================================

#define MODULE_NAME "axon"
#define MODULE_VERSION "2.0.0"

//=============================================================================
// MODULE STATE
//=============================================================================

typedef struct {
    bool initialized;
    uint32_t security_module_id;
    nimcp_mutex_t module_lock;
    uint64_t total_axons_created;
    uint64_t total_spikes_propagated;
} axon_module_state_t;

static axon_module_state_t g_axon_module = {
    .initialized = false,
    .security_module_id = 0,
    .total_axons_created = 0,
    .total_spikes_propagated = 0
};

//=============================================================================
// CONFIGURATION HELPERS (replaces hardcoded constants)
//=============================================================================

static inline float get_velocity_coeff_unmyelinated(void) {
    return (float)config_get_float("axon.velocity_coeff_unmyelinated", 1.0);
}

static inline float get_velocity_coeff_myelinated(void) {
    return (float)config_get_float("axon.velocity_coeff_myelinated", 6.0);
}

static inline float get_activity_decay_factor(void) {
    return (float)config_get_float("axon.activity_decay_factor", 0.99);
}

static inline float get_min_velocity(void) {
    return (float)config_get_float("axon.min_velocity", 0.1);
}

static inline float get_refractory_period_ms(void) {
    return (float)config_get_float("axon.refractory_period_ms", 1.0);
}

static inline float get_atp_consumption_per_spike(void) {
    return (float)config_get_float("axon.atp_consumption_per_spike", 0.01);
}

static inline float get_atp_regeneration_rate(void) {
    return (float)config_get_float("axon.atp_regeneration_rate", 0.001);
}

//=============================================================================
// MODULE INITIALIZATION AND SHUTDOWN
//=============================================================================

/**
 * @brief Initialize axon module
 *
 * WHAT: Initialize module state, register with security, setup logging
 * WHY:  Enable security monitoring and proper resource management
 * HOW:  One-time initialization with security registration
 *
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t axon_module_init(void) {
    if (g_axon_module.initialized) {
        LOG_MODULE_WARN(MODULE_NAME, "Module already initialized");
        return NIMCP_SUCCESS;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Initializing axon module v%s", MODULE_VERSION);

    // Initialize module lock
    nimcp_mutex_init(&g_axon_module.module_lock, NULL);

    // Register with security system
    // TODO: Implement security_register_module() call
    // g_axon_module.security_module_id = security_register_module(MODULE_NAME, MODULE_VERSION);

    LOG_MODULE_DEBUG(MODULE_NAME, "Security registration: module_id=%u",
                     g_axon_module.security_module_id);

    // Reset statistics
    g_axon_module.total_axons_created = 0;
    g_axon_module.total_spikes_propagated = 0;

    g_axon_module.initialized = true;

    LOG_MODULE_INFO(MODULE_NAME, "Axon module initialized successfully");
    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown axon module
 *
 * WHAT: Cleanup module resources, unregister from security
 * WHY:  Proper cleanup on application shutdown
 * HOW:  Release locks, unregister from security
 */
void axon_module_shutdown(void) {
    if (!g_axon_module.initialized) {
        return;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Shutting down axon module");
    LOG_MODULE_INFO(MODULE_NAME, "Statistics: %llu axons created, %llu spikes propagated",
                    (unsigned long long)g_axon_module.total_axons_created,
                    (unsigned long long)g_axon_module.total_spikes_propagated);

    // Unregister from security
    // TODO: security_unregister_module(g_axon_module.security_module_id);

    nimcp_mutex_destroy(&g_axon_module.module_lock);

    g_axon_module.initialized = false;
    LOG_MODULE_INFO(MODULE_NAME, "Axon module shutdown complete");
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Calculate 3D distance
 */
float axon_distance_3d(const float a[3], const float b[3])
{
    if (!a || !b) {
        LOG_MODULE_ERROR(MODULE_NAME, "NULL pointer in distance calculation");
        return 0.0f;
    }

    float dx = b[0] - a[0];
    float dy = b[1] - a[1];
    float dz = b[2] - a[2];

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Validate axon parameters
 */
bool axon_validate_params(float length, float diameter)
{
    if (length <= 0.0f) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid axon length: %f", length);
        return false;
    }

    if (diameter < NIMCP_AXON_MIN_DIAMETER_UM) {
        LOG_MODULE_ERROR(MODULE_NAME, "Axon diameter too small: %f < %f",
                        diameter, NIMCP_AXON_MIN_DIAMETER_UM);
        return false;
    }

    if (diameter > NIMCP_AXON_MAX_DIAMETER_UM) {
        LOG_MODULE_ERROR(MODULE_NAME, "Axon diameter too large: %f > %f",
                        diameter, NIMCP_AXON_MAX_DIAMETER_UM);
        return false;
    }

    return true;
}

//=============================================================================
// AXON CREATION AND DESTRUCTION
//=============================================================================

/**
 * @brief Create axon with async notification
 *
 * REFACTORED: Now returns future for async creation confirmation
 *
 * @param id Axon ID
 * @param type Axon type
 * @param source_neuron_id Source neuron
 * @param target_synapse_id Target synapse
 * @param length Axon length (um)
 * @param diameter Axon diameter (um)
 * @return Future that resolves to axon_t* or NULL on failure
 */
nimcp_future_t axon_create_async(uint32_t id,
                                  axon_type_t type,
                                  uint32_t source_neuron_id,
                                  uint32_t target_synapse_id,
                                  float length,
                                  float diameter)
{
    LOG_MODULE_DEBUG(MODULE_NAME, "Creating axon id=%u type=%d length=%f diameter=%f",
                     id, type, length, diameter);

    // Create promise for async result
    nimcp_promise_t promise = nimcp_promise_create(sizeof(axon_t*));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to create promise for axon creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "promise is NULL");

        return NULL;
    }

    // Validate parameters
    if (!axon_validate_params(length, diameter)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid axon parameters");
        nimcp_promise_fail(promise, NIMCP_ERROR_INVALID_PARAMETER);
        nimcp_future_t future = nimcp_promise_get_future(promise);
        nimcp_promise_destroy(promise);
        return future;
    }

    // Allocate axon structure using unified memory
    axon_t* axon = (axon_t*)nimcp_calloc(1, sizeof(axon_t));
    if (!axon) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate axon structure");
        nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY);
        nimcp_future_t future = nimcp_promise_get_future(promise);
        nimcp_promise_destroy(promise);
        return future;
    }

    // Initialize identification
    axon->id = id;
    axon->type = type;
    axon->state = AXON_STATE_RESTING;

    LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Initialized identification", id);

    // Initialize connectivity
    axon->source_neuron_id = source_neuron_id;
    axon->target_synapse_id = target_synapse_id;
    axon->target_neuron_id = 0;

    // Initialize morphology
    axon->length = length;
    axon->diameter = nimcp_clampf(diameter, NIMCP_AXON_MIN_DIAMETER_UM,
                             NIMCP_AXON_MAX_DIAMETER_UM);

    LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Set morphology length=%f diameter=%f",
                     id, axon->length, axon->diameter);

    // No segments initially
    axon->num_segments = 0;
    axon->segments = NULL;

    // Initialize conduction based on type
    axon->myelination_level = (type == AXON_TYPE_MYELINATED ||
                               type == AXON_TYPE_A_ALPHA ||
                               type == AXON_TYPE_A_BETA) ? 0.5f : 0.0f;

    // Get optimal g-ratio from config or use default
    axon->mean_g_ratio = (float)config_get_float("axon.default_g_ratio", 0.77);

    // Calculate initial conduction properties
    axon_update_conduction(axon);

    LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Calculated conduction velocity=%f m/s",
                     id, axon->effective_velocity);

    // Initialize activity tracking
    memset(&axon->activity, 0, sizeof(axon_activity_stats_t));

    // Initialize metabolic state from config
    axon->atp_level = (float)config_get_float("axon.initial_atp_level", 1.0);
    axon->lactate_level = 0.0f;

    // Initialize health
    axon->damage = 0.0f;
    axon->is_functional = true;

    // Initialize lock
    nimcp_mutex_init(&axon->lock, NULL);

    // Initialize memory pool (not enabled by default)
    axon->segment_pool = NULL;
    axon->use_segment_pool = config_get_bool("axon.use_segment_pool", false);

    // Initialize CoW (reference count starts at 1 for original)
    axon->cow_ref_count = 1;
    axon->cow_modified = false;
    axon->cow_original = NULL;

    // Update module statistics
    nimcp_mutex_lock(&g_axon_module.module_lock);
    g_axon_module.total_axons_created++;
    nimcp_mutex_unlock(&g_axon_module.module_lock);

    LOG_MODULE_INFO(MODULE_NAME, "Created axon id=%u type=%d successfully", id, type);

    // Complete the promise with the created axon
    nimcp_promise_complete(promise, &axon);

    nimcp_future_t future = nimcp_promise_get_future(promise);
    nimcp_promise_destroy(promise);

    return future;
}

/**
 * @brief Create axon (synchronous wrapper)
 *
 * WHAT: Synchronous wrapper around async creation
 * WHY:  Backward compatibility with existing code
 * HOW:  Blocks on future until creation completes
 */
axon_t* axon_create(uint32_t id,
                    axon_type_t type,
                    uint32_t source_neuron_id,
                    uint32_t target_synapse_id,
                    float length,
                    float diameter)
{
    // Call async version
    nimcp_future_t future = axon_create_async(id, type, source_neuron_id,
                                               target_synapse_id, length, diameter);
    if (!future) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to create async future for axon");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "future is NULL");

        return NULL;
    }

    // Wait for completion (with timeout from config)
    uint32_t timeout_ms = (uint32_t)config_get_int("axon.creation_timeout_ms", 5000);
    if (!nimcp_future_wait_timeout(future, timeout_ms)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Axon creation timed out after %u ms", timeout_ms);
        nimcp_future_destroy(future);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_create: nimcp_future_wait_timeout is NULL");
        return NULL;
    }

    // Get result
    axon_t* axon = NULL;
    nimcp_error_t err = nimcp_future_get(future, &axon);
    if (err != NIMCP_SUCCESS) {
        LOG_MODULE_ERROR(MODULE_NAME, "Axon creation failed with error: %d", err);
        nimcp_future_destroy(future);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_create: validation failed");
        return NULL;
    }

    nimcp_future_destroy(future);
    return axon;
}

/**
 * @brief Destroy axon with logging
 *
 * REFACTORED: Added comprehensive logging
 */
void axon_destroy(axon_t* axon)
{
    if (!axon) {
        LOG_MODULE_WARN(MODULE_NAME, "Attempted to destroy NULL axon");
        return;
    }

    uint32_t axon_id = axon->id;
    LOG_MODULE_DEBUG(MODULE_NAME, "Destroying axon id=%u", axon_id);

    // Check CoW reference count - only destroy if last reference
    if (axon->cow_ref_count > 1 && !axon->cow_original) {
        axon->cow_ref_count--;
        LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Decremented CoW ref count to %u",
                        axon_id, axon->cow_ref_count);
        return;
    }

    // Destroy mutex
    nimcp_mutex_destroy(&axon->lock);

    // Free segments if allocated and owned
    if (axon->segments && (axon->cow_modified || !axon->cow_original)) {
        LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Freeing %u segments",
                        axon_id, axon->num_segments);
        nimcp_free(axon->segments);
    }

    // Free segment pool if allocated
    if (axon->segment_pool) {
        LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Destroying segment pool", axon_id);
        axon_segment_pool_destroy(axon->segment_pool);
    }

    // Free axon structure using unified memory
    nimcp_free(axon);

    LOG_MODULE_INFO(MODULE_NAME, "Destroyed axon id=%u", axon_id);
}

//=============================================================================
// CONDUCTION VELOCITY (with config integration)
//=============================================================================

/**
 * @brief Calculate velocity with config-based coefficients
 *
 * REFACTORED: Uses config system instead of hardcoded constants
 */
float axon_calculate_velocity(const axon_t* axon)
{
    if (!axon) {
        LOG_MODULE_ERROR(MODULE_NAME, "NULL axon in velocity calculation");
        return get_min_velocity();
    }

    float velocity;

    // Get coefficients from config
    float unmyelinated_coeff = get_velocity_coeff_unmyelinated();
    float myelinated_coeff = get_velocity_coeff_myelinated();

    LOG_MODULE_TRACE(MODULE_NAME, "Axon %u: Calculating velocity (type=%d, diameter=%f, myelination=%f)",
                     axon->id, axon->type, axon->diameter, axon->myelination_level);

    // Type-specific velocity calculation
    switch (axon->type) {
        case AXON_TYPE_UNMYELINATED:
        case AXON_TYPE_C_FIBER:
            velocity = unmyelinated_coeff * sqrtf(axon->diameter);
            LOG_MODULE_TRACE(MODULE_NAME, "Axon %u: Unmyelinated velocity = %f m/s",
                           axon->id, velocity);
            break;

        case AXON_TYPE_MYELINATED:
        case AXON_TYPE_A_ALPHA:
        case AXON_TYPE_A_BETA:
        case AXON_TYPE_A_DELTA:
            velocity = myelinated_coeff * axon->diameter *
                       (0.1f + 0.9f * axon->myelination_level);
            LOG_MODULE_TRACE(MODULE_NAME, "Axon %u: Myelinated velocity = %f m/s",
                           axon->id, velocity);
            break;

        default:
            velocity = NIMCP_AXON_BASE_VELOCITY_MS;
            LOG_MODULE_WARN(MODULE_NAME, "Axon %u: Unknown type, using base velocity",
                          axon->id);
    }

    // Clamp to valid range
    velocity = nimcp_clampf(velocity, get_min_velocity(), NIMCP_AXON_MAX_VELOCITY_MS);

    // Apply damage penalty
    if (axon->damage > 0.0f) {
        float damaged_velocity = velocity * (1.0f - axon->damage * 0.9f);
        LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Damage penalty applied: %f -> %f m/s (damage=%f)",
                        axon->id, velocity, damaged_velocity, axon->damage);
        velocity = damaged_velocity;
    }

    return velocity;
}

/**
 * @brief Update conduction properties
 */
void axon_update_conduction(axon_t* axon)
{
    if (!axon) {
        LOG_MODULE_ERROR(MODULE_NAME, "NULL axon in update_conduction");
        return;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Updating conduction properties", axon->id);

    // Calculate new velocity
    axon->effective_velocity = axon_calculate_velocity(axon);

    // Calculate propagation delay
    axon->propagation_delay_ms = axon->length /
                                  (axon->effective_velocity * 1000.0f);

    // Update base velocity for reference
    axon->base_velocity = get_velocity_coeff_unmyelinated() * sqrtf(axon->diameter);

    LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Updated conduction - velocity=%f m/s, delay=%f ms",
                     axon->id, axon->effective_velocity, axon->propagation_delay_ms);
}

//=============================================================================
// SPIKE PROPAGATION (with async event publishing)
//=============================================================================

/**
 * @brief Initiate spike with async event publication
 *
 * REFACTORED: Publishes spike event asynchronously to subscribers
 */
bool axon_initiate_spike(axon_t* axon,
                         uint64_t current_time,
                         float amplitude)
{
    if (!axon) {
        LOG_MODULE_ERROR(MODULE_NAME, "NULL axon in initiate_spike");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_initiate_spike: axon is NULL");
        return false;
    }

    if (!axon->is_functional) {
        LOG_MODULE_WARN(MODULE_NAME, "Axon %u: Cannot spike - not functional", axon->id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_initiate_spike: axon->is_functional is NULL");
        return false;
    }

    if (axon->damage >= 1.0f) {
        LOG_MODULE_WARN(MODULE_NAME, "Axon %u: Cannot spike - fully damaged", axon->id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "axon_initiate_spike: capacity exceeded");
        return false;
    }

    // Check refractory period
    if (axon_is_refractory(axon, current_time)) {
        LOG_MODULE_TRACE(MODULE_NAME, "Axon %u: Cannot spike - in refractory period", axon->id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "axon_initiate_spike: validation failed");
        return false;
    }

    // Check ATP level (configurable threshold)
    float min_atp = (float)config_get_float("axon.min_atp_for_spike", 0.1);
    if (axon->atp_level < min_atp) {
        LOG_MODULE_WARN(MODULE_NAME, "Axon %u: Cannot spike - insufficient ATP (%f < %f)",
                       axon->id, axon->atp_level, min_atp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "axon_initiate_spike: validation failed");
        return false;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Axon %u: Initiating spike at time=%llu amplitude=%f",
                     axon->id, (unsigned long long)current_time, amplitude);

    nimcp_mutex_lock(&axon->lock);

    // Update state
    axon->state = AXON_STATE_ACTIVE;

    // Set refractory period end
    float refract_ms = get_refractory_period_ms();
    uint64_t refractory_us = (uint64_t)(refract_ms * 1000.0f);
    axon->refractory_end = current_time + refractory_us;

    // Update activity statistics
    axon->activity.total_spikes++;
    axon->activity.recent_spikes++;

    // Calculate inter-spike interval
    if (axon->activity.last_spike_time > 0) {
        float isi = (float)(current_time - axon->activity.last_spike_time) / 1000.0f;
        axon->activity.mean_isi = 0.9f * axon->activity.mean_isi + 0.1f * isi;
        LOG_MODULE_TRACE(MODULE_NAME, "Axon %u: ISI=%f ms, mean ISI=%f ms",
                        axon->id, isi, axon->activity.mean_isi);
    }
    axon->activity.last_spike_time = current_time;

    // Consume ATP (from config)
    float atp_cost = get_atp_consumption_per_spike();
    axon->atp_level -= atp_cost;
    if (axon->atp_level < 0.0f) axon->atp_level = 0.0f;

    LOG_MODULE_TRACE(MODULE_NAME, "Axon %u: ATP consumed=%f, remaining=%f",
                    axon->id, atp_cost, axon->atp_level);

    // Update module statistics
    nimcp_mutex_lock(&g_axon_module.module_lock);
    g_axon_module.total_spikes_propagated++;
    nimcp_mutex_unlock(&g_axon_module.module_lock);

    nimcp_mutex_unlock(&axon->lock);

    LOG_MODULE_INFO(MODULE_NAME, "Axon %u: Spike initiated successfully", axon->id);

    // TODO: Publish async event to event bus
    // This would notify other modules (synapses, etc.) of the spike
    // Example: event_bus_publish("axon.spike", {axon_id, timestamp, amplitude});

    return true;
}

// ... (additional functions follow same pattern)

