/**
 * @file nimcp_portia_attention.c
 * @brief Implementation of Attention-Based Resource Allocation
 *
 * ARCHITECTURE:
 * - Thread-safe operations with mutex protection
 * - Fair allocation algorithm with priority weighting
 * - Smooth transitions with exponential smoothing
 * - Hysteresis to prevent oscillation
 * - Bio-async event broadcasting for coordination
 *
 * ALLOCATION ALGORITHM DETAILS:
 * 1. Calculate combined score = salience * (priority / max_priority)
 * 2. Sort targets by score (descending)
 * 3. Allocate minimum requirements first
 * 4. Distribute remaining budget proportionally to scores
 * 5. Cap at maximum allocation limits
 * 6. Apply hysteresis: only change if delta > threshold
 * 7. Smooth transitions: new = alpha * new + (1-alpha) * old
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "portia/nimcp_portia_attention.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_attention)

//=============================================================================
// Constants
//=============================================================================

#define PORTIA_ATTENTION_MAGIC 0x504F5254  // 'PORT'
#define MIN_SALIENCE 0.0F
#define MAX_SALIENCE 1.0F
#define MIN_ALLOCATION 0.0f
#define MAX_ALLOCATION 1.0f
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal attention state structure
 */
struct portia_attention_state_struct {
    uint32_t magic;                      /**< Magic validation */

    /* Configuration */
    portia_attention_config_t config;

    /* Resources */
    attention_resource_t* resources;
    uint32_t resource_count;
    float total_budget;

    /* State */
    float attention_decay_rate;
    uint64_t last_update_ms;
    uint64_t last_reallocation_ms;

    /* Statistics */
    portia_attention_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t mutex;

    /* Bio-async */
    uint32_t bio_module_id;
    bool bio_async_enabled;
};

/**
 * @brief Internal structure for sorting resources
 */
typedef struct {
    uint32_t index;
    float score;
    attention_resource_t* resource;
} resource_sort_entry_t;

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Validate attention state
 */
static bool validate_state(portia_attention_state_t state) {
    if (!state) {
        LOG_ERROR("Attention state is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_state: state is NULL");
        return false;
    }

    bbb_validation_result_t result;
    if (!bbb_validate_pointer(NULL, state, sizeof(*state), &result)) {
        LOG_ERROR("Invalid attention state pointer: %s", result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_state: bbb_validate_pointer is NULL");
        return false;
    }

    if (state->magic != PORTIA_ATTENTION_MAGIC) {
        LOG_ERROR("Invalid magic number: 0x%08x (expected 0x%08x)",
                  state->magic, PORTIA_ATTENTION_MAGIC);
        return false;
    }

    return true;
}

/**
 * @brief Broadcast bio-async event
 */
static void broadcast_event(portia_attention_state_t state,
                            attention_event_t event,
                            attention_target_t target,
                            float value) {
    if (!state->bio_async_enabled) {
        return;
    }

    LOG_DEBUG("Broadcasting attention event: %s for target %s (value=%.3f)",
              portia_attention_event_name(event),
              portia_attention_target_name(target),
              value);

    // Create and send bio-async message
    // Note: Actual bio-async integration would go here
    // For now, just log the event
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    return nimcp_time_monotonic_ms();
}

/**
 * @brief Comparison function for sorting resources
 */
static int compare_resources(const void* a, const void* b) {
    const resource_sort_entry_t* entry_a = (const resource_sort_entry_t*)a;
    const resource_sort_entry_t* entry_b = (const resource_sort_entry_t*)b;

    // Sort by score descending
    if (entry_a->score > entry_b->score) return -1;
    if (entry_a->score < entry_b->score) return 1;
    return 0;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

portia_attention_config_t portia_attention_default_config(void) {
    portia_attention_config_t config = {
        .reallocation_threshold = 0.05F,   // 5% change triggers reallocation
        .decay_rate_per_second = 0.1F,     // 10% decay per second
        .update_interval_ms = 100,          // Update every 100ms
        .enable_preemption = true,
        .preemption_threshold = 0.3F,       // 30% salience difference
        .hysteresis_factor = 0.2F,          // 20% hysteresis band
        .smoothing_alpha = 0.3F             // 30% new value, 70% old
    };
    return config;
}

portia_attention_state_t portia_attention_init(
    const portia_attention_config_t* config,
    uint32_t resource_count,
    float total_budget) {

    LOG_INFO("Initializing Portia attention system (resources=%u, budget=%.3f)",
             resource_count, total_budget);

    // Validate parameters
    if (resource_count == 0) {
        LOG_ERROR("Resource count cannot be zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Resource count cannot be zero in portia_attention_init");
        return NULL;
    }

    if (total_budget <= 0.0F || total_budget > 1.0F) {
        LOG_ERROR("Invalid total budget: %.3f (must be in (0,1])", total_budget);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid total budget in portia_attention_init");
        return NULL;
    }

    bbb_validation_result_t result;
    if (config && !bbb_validate_pointer(NULL, config, sizeof(*config), &result)) {
        LOG_ERROR("Invalid config pointer: %s", result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid config pointer in portia_attention_init");
        return NULL;
    }

    // Allocate state
    portia_attention_state_t state = nimcp_calloc(1, sizeof(*state));
    if (!state) {
        LOG_ERROR("Failed to allocate attention state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate attention state");
        return NULL;
    }

    state->magic = PORTIA_ATTENTION_MAGIC;
    state->resource_count = resource_count;
    state->total_budget = total_budget;

    // Copy configuration
    if (config) {
        memcpy(&state->config, config, sizeof(state->config));
    } else {
        state->config = portia_attention_default_config();
    }

    // Validate configuration
    state->config.reallocation_threshold = nimcp_clampf(
        state->config.reallocation_threshold, 0.0F, 1.0F);
    state->config.decay_rate_per_second = nimcp_clampf(
        state->config.decay_rate_per_second, 0.0F, 1.0F);
    state->config.hysteresis_factor = nimcp_clampf(
        state->config.hysteresis_factor, 0.0F, 1.0F);
    state->config.smoothing_alpha = nimcp_clampf(
        state->config.smoothing_alpha, 0.0F, 1.0F);

    LOG_DEBUG("Config: threshold=%.3f, decay=%.3f/s, interval=%ums",
              state->config.reallocation_threshold,
              state->config.decay_rate_per_second,
              state->config.update_interval_ms);

    // Allocate resources array
    state->resources = nimcp_calloc(resource_count, sizeof(attention_resource_t));
    if (!state->resources) {
        LOG_ERROR("Failed to allocate resources array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate resources array in portia_attention_init");
        nimcp_free(state);
        return NULL;
    }

    // Initialize resources with equal allocation
    float initial_allocation = total_budget / (float)resource_count;
    for (uint32_t i = 0; i < resource_count; i++) {
        state->resources[i].target = (attention_target_t)i;
        state->resources[i].salience = 0.5F;  // Neutral salience
        state->resources[i].current_allocation = initial_allocation;
        state->resources[i].requested_allocation = initial_allocation;
        state->resources[i].min_allocation = 0.0F;
        state->resources[i].max_allocation = 1.0F;
        state->resources[i].priority = 1;
        state->resources[i].last_update_ms = 0;
    }

    LOG_DEBUG("Initialized %u resources with %.3f allocation each",
              resource_count, initial_allocation);

    // Initialize timing
    state->last_update_ms = get_current_time_ms();
    state->last_reallocation_ms = state->last_update_ms;
    state->attention_decay_rate = state->config.decay_rate_per_second;

    // Initialize mutex
    nimcp_mutex_init(&state->mutex, NULL);


    // Initialize statistics
    memset(&state->stats, 0, sizeof(state->stats));
    state->stats.total_allocated = total_budget;
    state->stats.avg_salience = 0.5F;

    // Bio-async setup
    state->bio_module_id = BIO_MODULE_ATTENTION;
    state->bio_async_enabled = false;  // Will be enabled when registered

    LOG_INFO("Portia attention system initialized successfully");

    return state;
}

void portia_attention_destroy(portia_attention_state_t state) {
    if (!validate_state(state)) {
        return;
    }

    LOG_INFO("Destroying Portia attention system");

    // Log final statistics
    LOG_INFO("Final stats: salience_updates=%lu, reallocations=%lu, preemptions=%lu",
             state->stats.salience_updates,
             state->stats.reallocations,
             state->stats.preemptions);

    // Destroy mutex
    nimcp_mutex_destroy(&state->mutex);

    // Free resources
    if (state->resources) {
        nimcp_free(state->resources);
    }

    // Clear magic and free state
    state->magic = 0;
    nimcp_free(state);

    LOG_DEBUG("Portia attention system destroyed");
}

//=============================================================================
// Salience Management
//=============================================================================

int portia_attention_update_salience(
    portia_attention_state_t state,
    attention_target_t target,
    float salience) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_update_salience: invalid state");
        return -1;
    }

    if (target >= state->resource_count) {
        LOG_ERROR("Invalid target: %d (max=%u)", target, state->resource_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "portia_attention_update_salience: target out of range");
        return -1;
    }

    // Validate salience range
    if (salience < MIN_SALIENCE || salience > MAX_SALIENCE) {
        LOG_ERROR("Salience %.3f out of range [%.1f, %.1f]",
                  salience, MIN_SALIENCE, MAX_SALIENCE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_update_salience: salience out of range");
        return -1;
    }

    nimcp_mutex_lock(&state->mutex);

    attention_resource_t* resource = &state->resources[target];
    float old_salience = resource->salience;

    // Update salience
    resource->salience = salience;
    resource->last_update_ms = get_current_time_ms();

    // Update statistics
    state->stats.salience_updates++;

    // Recalculate average salience
    float total_salience = 0.0F;
    for (uint32_t i = 0; i < state->resource_count; i++) {
        total_salience += state->resources[i].salience;
    }
    state->stats.avg_salience = total_salience / (float)state->resource_count;

    nimcp_mutex_unlock(&state->mutex);

    LOG_DEBUG("Updated salience for %s: %.3f -> %.3f (avg=%.3f)",
              portia_attention_target_name(target),
              old_salience, salience, state->stats.avg_salience);

    // Broadcast event
    broadcast_event(state, ATTENTION_EVENT_SALIENCE_UPDATED, target, salience);

    return 0;
}

int portia_attention_decay(
    portia_attention_state_t state,
    uint64_t current_time_ms) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_decay: invalid state");
        return -1;
    }

    nimcp_mutex_lock(&state->mutex);

    // Calculate time elapsed since last update
    uint64_t elapsed_ms = current_time_ms - state->last_update_ms;
    if (elapsed_ms == 0) {
        nimcp_mutex_unlock(&state->mutex);
        return 0;
    }

    float elapsed_sec = (float)elapsed_ms / 1000.0F;
    float decay_factor = expf(-state->attention_decay_rate * elapsed_sec);

    LOG_DEBUG("Applying salience decay: elapsed=%.3fs, factor=%.3f",
              elapsed_sec, decay_factor);

    // Apply decay to all resources
    for (uint32_t i = 0; i < state->resource_count; i++) {
        float old_salience = state->resources[i].salience;
        state->resources[i].salience *= decay_factor;

        // Don't let salience go to absolute zero
        if (state->resources[i].salience < EPSILON) {
            state->resources[i].salience = EPSILON;
        }

        LOG_DEBUG("Decayed %s: %.3f -> %.3f",
                  portia_attention_target_name((attention_target_t)i),
                  old_salience, state->resources[i].salience);
    }

    state->last_update_ms = current_time_ms;

    nimcp_mutex_unlock(&state->mutex);

    return 0;
}

float portia_attention_get_salience(
    portia_attention_state_t state,
    attention_target_t target) {

    if (!validate_state(state)) {
        return -1.0F;
    }

    if (target >= state->resource_count) {
        LOG_ERROR("Invalid target: %d", target);
        return -1.0F;
    }

    nimcp_mutex_lock(&state->mutex);
    float salience = state->resources[target].salience;
    nimcp_mutex_unlock(&state->mutex);

    return salience;
}

//=============================================================================
// Resource Allocation
//=============================================================================

int portia_attention_reallocate(
    portia_attention_state_t state,
    bool force_reallocation) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_reallocate: invalid state");
        return -1;
    }

    nimcp_mutex_lock(&state->mutex);

    uint64_t current_time = get_current_time_ms();

    // Check if we should reallocate
    if (!force_reallocation) {
        uint64_t time_since_last = current_time - state->last_reallocation_ms;
        if (time_since_last < state->config.update_interval_ms) {
            nimcp_mutex_unlock(&state->mutex);
            return 0;  // Too soon
        }
    }

    LOG_DEBUG("Performing resource reallocation (forced=%d)", force_reallocation);

    // Create sort entries
    resource_sort_entry_t* sort_entries = nimcp_calloc(
        state->resource_count, sizeof(resource_sort_entry_t));
    if (!sort_entries) {
        LOG_ERROR("Failed to allocate sort entries");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sort entries in portia_attention_reallocate");
        nimcp_mutex_unlock(&state->mutex);
        return -1;
    }

    // Calculate scores and find max priority
    uint32_t max_priority = 0;
    for (uint32_t i = 0; i < state->resource_count; i++) {
        if (state->resources[i].priority > max_priority) {
            max_priority = state->resources[i].priority;
        }
    }

    if (max_priority == 0) max_priority = 1;

    float total_score = 0.0F;
    for (uint32_t i = 0; i < state->resource_count; i++) {
        sort_entries[i].index = i;
        sort_entries[i].resource = &state->resources[i];

        // Score = salience * normalized_priority
        float normalized_priority = (float)state->resources[i].priority /
                                    (float)max_priority;
        sort_entries[i].score = state->resources[i].salience * normalized_priority;
        total_score += sort_entries[i].score;

        LOG_DEBUG("Resource %s: salience=%.3f, priority=%u, score=%.3f",
                  portia_attention_target_name((attention_target_t)i),
                  state->resources[i].salience,
                  state->resources[i].priority,
                  sort_entries[i].score);
    }

    // Sort by score (descending)
    qsort(sort_entries, state->resource_count, sizeof(resource_sort_entry_t),
          compare_resources);

    // Phase 1: Allocate minimum requirements
    float remaining_budget = state->total_budget;
    for (uint32_t i = 0; i < state->resource_count; i++) {
        attention_resource_t* res = sort_entries[i].resource;
        float min_alloc = res->min_allocation;
        if (min_alloc > remaining_budget) {
            min_alloc = remaining_budget;
        }
        remaining_budget -= min_alloc;
    }

    LOG_DEBUG("After minimum allocations: remaining=%.3f", remaining_budget);

    // Phase 2: Distribute remaining budget proportionally
    float* new_allocations = nimcp_calloc(state->resource_count, sizeof(float));
    if (!new_allocations) {
        LOG_ERROR("Failed to allocate new_allocations array");
        nimcp_free(sort_entries);
        nimcp_mutex_unlock(&state->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_allocations in portia_attention_reallocate");
        return -1;
    }

    for (uint32_t i = 0; i < state->resource_count; i++) {
        attention_resource_t* res = sort_entries[i].resource;
        uint32_t idx = sort_entries[i].index;

        // Start with minimum
        new_allocations[idx] = res->min_allocation;

        // Add proportional share
        if (total_score > EPSILON && remaining_budget > EPSILON) {
            float proportion = sort_entries[i].score / total_score;
            float additional = remaining_budget * proportion;
            new_allocations[idx] += additional;
        }

        // Respect maximum
        if (new_allocations[idx] > res->max_allocation) {
            float excess = new_allocations[idx] - res->max_allocation;
            new_allocations[idx] = res->max_allocation;
            remaining_budget += excess;  // Return excess to pool
        }

        LOG_DEBUG("Computed allocation for %s: %.3f (min=%.3f, max=%.3f)",
                  portia_attention_target_name((attention_target_t)idx),
                  new_allocations[idx],
                  res->min_allocation,
                  res->max_allocation);
    }

    // Phase 3: Apply hysteresis and smoothing
    bool any_changed = false;
    float threshold = state->config.reallocation_threshold;
    float alpha = state->config.smoothing_alpha;

    for (uint32_t i = 0; i < state->resource_count; i++) {
        attention_resource_t* res = &state->resources[i];
        float old_alloc = res->current_allocation;
        float new_alloc = new_allocations[i];

        // Apply hysteresis: only change if delta exceeds threshold
        float delta = fabsf(new_alloc - old_alloc);
        if (delta < threshold && !force_reallocation) {
            continue;  // Change too small
        }

        // Apply exponential smoothing for gradual transitions
        float smoothed = alpha * new_alloc + (1.0F - alpha) * old_alloc;

        res->current_allocation = smoothed;
        any_changed = true;

        LOG_INFO("Reallocated %s: %.3f -> %.3f (delta=%.3f)",
                 portia_attention_target_name((attention_target_t)i),
                 old_alloc, smoothed, delta);

        // Broadcast event
        broadcast_event(state, ATTENTION_EVENT_ALLOCATION_CHANGED,
                       (attention_target_t)i, smoothed);
    }

    // Update statistics
    if (any_changed) {
        state->stats.reallocations++;
        state->last_reallocation_ms = current_time;

        // Calculate total allocated
        float total = 0.0F;
        for (uint32_t i = 0; i < state->resource_count; i++) {
            total += state->resources[i].current_allocation;
        }
        state->stats.total_allocated = total;

        LOG_INFO("Reallocation complete: total=%.3f/%.3f",
                 total, state->total_budget);
    } else {
        LOG_DEBUG("No changes triggered reallocation");
    }

    nimcp_free(new_allocations);
    nimcp_free(sort_entries);
    nimcp_mutex_unlock(&state->mutex);

    return 0;
}

int portia_attention_request(
    portia_attention_state_t state,
    attention_target_t target,
    float amount) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_request: invalid state");
        return -1;
    }

    if (target >= state->resource_count) {
        LOG_ERROR("Invalid target: %d", target);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "portia_attention_request: target out of range");
        return -1;
    }

    amount = nimcp_clampf(amount, MIN_ALLOCATION, MAX_ALLOCATION);

    nimcp_mutex_lock(&state->mutex);

    attention_resource_t* res = &state->resources[target];
    res->requested_allocation = amount;
    state->stats.requests++;

    nimcp_mutex_unlock(&state->mutex);

    LOG_DEBUG("Resource request from %s: %.3f",
              portia_attention_target_name(target), amount);

    broadcast_event(state, ATTENTION_EVENT_RESOURCES_REQUESTED, target, amount);

    // Trigger reallocation check
    portia_attention_reallocate(state, false);

    return 0;
}

int portia_attention_release(
    portia_attention_state_t state,
    attention_target_t target,
    float amount) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_release: invalid state");
        return -1;
    }

    if (target >= state->resource_count) {
        LOG_ERROR("Invalid target: %d", target);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "portia_attention_release: target out of range");
        return -1;
    }

    amount = nimcp_clampf(amount, MIN_ALLOCATION, MAX_ALLOCATION);

    nimcp_mutex_lock(&state->mutex);

    attention_resource_t* res = &state->resources[target];

    if (amount > res->current_allocation) {
        amount = res->current_allocation;
    }

    res->current_allocation -= amount;
    state->stats.releases++;

    nimcp_mutex_unlock(&state->mutex);

    LOG_DEBUG("Resource release from %s: %.3f (remaining=%.3f)",
              portia_attention_target_name(target),
              amount, res->current_allocation);

    broadcast_event(state, ATTENTION_EVENT_RESOURCES_RELEASED, target, amount);

    // Trigger reallocation to redistribute
    portia_attention_reallocate(state, false);

    return 0;
}

float portia_attention_get_allocation(
    portia_attention_state_t state,
    attention_target_t target) {

    if (!validate_state(state)) {
        return -1.0F;
    }

    if (target >= state->resource_count) {
        LOG_ERROR("Invalid target: %d", target);
        return -1.0F;
    }

    nimcp_mutex_lock(&state->mutex);
    float allocation = state->resources[target].current_allocation;
    nimcp_mutex_unlock(&state->mutex);

    return allocation;
}

int portia_attention_get_all_allocations(
    portia_attention_state_t state,
    attention_resource_t* resources,
    uint32_t max_count) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_get_all_allocations: invalid state");
        return -1;
    }

    bbb_validation_result_t result;
    if (!bbb_validate_pointer(NULL, resources,
                              max_count * sizeof(attention_resource_t), &result)) {
        LOG_ERROR("Invalid resources buffer: %s", result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_get_all_allocations: invalid buffer");
        return -1;
    }

    nimcp_mutex_lock(&state->mutex);

    uint32_t count = state->resource_count < max_count ?
                     state->resource_count : max_count;

    memcpy(resources, state->resources, count * sizeof(attention_resource_t));

    nimcp_mutex_unlock(&state->mutex);

    return (int)count;
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

int portia_attention_get_stats(
    portia_attention_state_t state,
    portia_attention_stats_t* stats) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_get_stats: invalid state");
        return -1;
    }

    bbb_validation_result_t result;
    if (!bbb_validate_pointer(NULL, stats, sizeof(*stats), &result)) {
        LOG_ERROR("Invalid stats pointer: %s", result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_get_stats: invalid stats pointer");
        return -1;
    }

    nimcp_mutex_lock(&state->mutex);
    memcpy(stats, &state->stats, sizeof(*stats));
    nimcp_mutex_unlock(&state->mutex);

    return 0;
}

void portia_attention_reset_stats(portia_attention_state_t state) {
    if (!validate_state(state)) {
        return;
    }

    nimcp_mutex_lock(&state->mutex);

    // Preserve some values
    float total_allocated = state->stats.total_allocated;
    float avg_salience = state->stats.avg_salience;

    memset(&state->stats, 0, sizeof(state->stats));

    state->stats.total_allocated = total_allocated;
    state->stats.avg_salience = avg_salience;

    nimcp_mutex_unlock(&state->mutex);

    LOG_DEBUG("Statistics reset");
}

bool portia_attention_needs_reallocation(
    portia_attention_state_t state,
    uint64_t current_time_ms) {

    if (!validate_state(state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_attention_needs_reallocation: invalid state");
        return false;
    }

    nimcp_mutex_lock(&state->mutex);

    // Check time interval
    uint64_t elapsed = current_time_ms - state->last_reallocation_ms;
    if (elapsed < state->config.update_interval_ms) {
        nimcp_mutex_unlock(&state->mutex);
        return false;  /* Normal: too early to reallocate */
    }

    // Check if any requested allocation differs significantly
    bool needs_realloc = false;
    for (uint32_t i = 0; i < state->resource_count; i++) {
        float delta = fabsf(state->resources[i].requested_allocation -
                           state->resources[i].current_allocation);
        if (delta > state->config.reallocation_threshold) {
            needs_realloc = true;
            break;
        }
    }

    nimcp_mutex_unlock(&state->mutex);

    return needs_realloc;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* portia_attention_target_name(attention_target_t target) {
    static const char* names[] = {
        "NEURONS",
        "MEMORY",
        "PROCESSING",
        "SENSORS",
        "COMMUNICATION"
    };

    if (target < ATTENTION_TARGET_COUNT) {
        return names[target];
    }
    return "UNKNOWN";
}

const char* portia_attention_event_name(attention_event_t event) {
    static const char* names[] = {
        "SALIENCE_UPDATED",
        "ALLOCATION_CHANGED",
        "RESOURCES_REQUESTED",
        "RESOURCES_RELEASED",
        "PREEMPTION_OCCURRED",
        "THRESHOLD_EXCEEDED"
    };

    if (event < ATTENTION_EVENT_COUNT) {
        return names[event];
    }
    return "UNKNOWN";
}

void portia_attention_print_state(portia_attention_state_t state) {
    if (!validate_state(state)) {
        return;
    }

    nimcp_mutex_lock(&state->mutex);

    LOG_INFO("=== Portia Attention State ===");
    LOG_INFO("Total Budget: %.3f", state->total_budget);
    LOG_INFO("Resources: %u", state->resource_count);
    LOG_INFO("Average Salience: %.3f", state->stats.avg_salience);
    LOG_INFO("Total Allocated: %.3f", state->stats.total_allocated);
    LOG_INFO("");
    LOG_INFO("%-15s %8s %8s %8s %8s",
             "Target", "Salience", "Current", "Request", "Priority");
    LOG_INFO("%-15s %8s %8s %8s %8s",
             "---------------", "--------", "--------", "--------", "--------");

    for (uint32_t i = 0; i < state->resource_count; i++) {
        attention_resource_t* res = &state->resources[i];
        LOG_INFO("%-15s %8.3f %8.3f %8.3f %8u",
                 portia_attention_target_name(res->target),
                 res->salience,
                 res->current_allocation,
                 res->requested_allocation,
                 res->priority);
    }

    LOG_INFO("");
    LOG_INFO("Statistics:");
    LOG_INFO("  Salience Updates: %lu", state->stats.salience_updates);
    LOG_INFO("  Reallocations:    %lu", state->stats.reallocations);
    LOG_INFO("  Preemptions:      %lu", state->stats.preemptions);
    LOG_INFO("  Requests:         %lu", state->stats.requests);
    LOG_INFO("  Releases:         %lu", state->stats.releases);
    LOG_INFO("===============================");

    nimcp_mutex_unlock(&state->mutex);
}
