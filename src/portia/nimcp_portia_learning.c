/**
 * @file nimcp_portia_learning.c
 * @brief Portia spider learning modes implementation
 *
 * Implements lightweight learning mechanisms inspired by Portia spiders:
 * - Habituation: Decrease response to repeated non-threatening stimuli
 * - Sensitization: Increase response to important stimuli
 * - Associative learning: Classical and operant conditioning
 * - Trial-and-error: Learn from outcomes
 * - Memory consolidation: Strengthen important memories, forget weak ones
 */

#include "portia/nimcp_portia_learning.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "portia_learning"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_learning)

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

// Default configuration values
#define DEFAULT_MAX_HABITUATION_ENTRIES 64
#define DEFAULT_MAX_ASSOCIATION_ENTRIES 128
#define DEFAULT_LEARNING_RATE NIMCP_LEARNING_RATE_COARSE
#define DEFAULT_FORGETTING_RATE 0.01f
#define DEFAULT_CONSOLIDATION_INTERVAL_MS 60000  // 1 minute
#define DEFAULT_HABITUATION_THRESHOLD 0.1f
#define DEFAULT_ASSOCIATION_THRESHOLD 0.05f

// Learning parameters
#define HABITUATION_DECAY_FACTOR 0.95f
#define SENSITIZATION_BOOST_MAX 2.0f
#define RECOVERY_TIME_MS 300000  // 5 minutes for spontaneous recovery
#define MIN_REINFORCEMENT_INTERVAL_MS 100

/**
 * Find habituation entry by stimulus ID
 */
static habituation_entry_t* find_habituation_entry(portia_learning_state_t* state,
                                                    uint32_t stimulus_id) {
    if (!state || !state->habituation_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_habituation_entry: required parameter is NULL (state, state->habituation_table)");
        return NULL;
    }

    for (uint32_t i = 0; i < state->habituation_capacity; i++) {
        if (state->habituation_table[i].is_active &&
            state->habituation_table[i].stimulus_id == stimulus_id) {
            return &state->habituation_table[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * Find association entry by stimulus and response IDs
 */
static association_entry_t* find_association_entry(portia_learning_state_t* state,
                                                    uint32_t stimulus_id,
                                                    uint32_t response_id) {
    if (!state || !state->association_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_association_entry: required parameter is NULL (state, state->association_table)");
        return NULL;
    }

    for (uint32_t i = 0; i < state->association_capacity; i++) {
        if (state->association_table[i].is_active &&
            state->association_table[i].stimulus_id == stimulus_id &&
            state->association_table[i].response_id == response_id) {
            return &state->association_table[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * Find least recently used habituation entry (for eviction)
 */
static habituation_entry_t* find_lru_habituation_entry(portia_learning_state_t* state) {
    if (!state || !state->habituation_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_lru_habituation_entry: required parameter is NULL (state, state->habituation_table)");
        return NULL;
    }

    habituation_entry_t* lru = NULL;
    uint64_t oldest_time = UINT64_MAX;

    for (uint32_t i = 0; i < state->habituation_capacity; i++) {
        if (state->habituation_table[i].is_active &&
            state->habituation_table[i].last_exposure_ms < oldest_time) {
            oldest_time = state->habituation_table[i].last_exposure_ms;
            lru = &state->habituation_table[i];
        }
    }
    return lru;
}

/**
 * Find least recently used association entry (for eviction)
 */
static association_entry_t* find_lru_association_entry(portia_learning_state_t* state) {
    if (!state || !state->association_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_lru_association_entry: required parameter is NULL (state, state->association_table)");
        return NULL;
    }

    association_entry_t* lru = NULL;
    uint64_t oldest_time = UINT64_MAX;

    for (uint32_t i = 0; i < state->association_capacity; i++) {
        if (state->association_table[i].is_active &&
            state->association_table[i].last_reinforcement_ms < oldest_time) {
            oldest_time = state->association_table[i].last_reinforcement_ms;
            lru = &state->association_table[i];
        }
    }
    return lru;
}

/**
 * Find empty habituation slot
 */
static habituation_entry_t* find_empty_habituation_slot(portia_learning_state_t* state) {
    if (!state || !state->habituation_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_habituation_slot: required parameter is NULL (state, state->habituation_table)");
        return NULL;
    }

    for (uint32_t i = 0; i < state->habituation_capacity; i++) {
        if (!state->habituation_table[i].is_active) {
            return &state->habituation_table[i];
        }
    }
    return NULL;  /* No empty slot available */
}

/**
 * Find empty association slot
 */
static association_entry_t* find_empty_association_slot(portia_learning_state_t* state) {
    if (!state || !state->association_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_association_slot: required parameter is NULL (state, state->association_table)");
        return NULL;
    }

    for (uint32_t i = 0; i < state->association_capacity; i++) {
        if (!state->association_table[i].is_active) {
            return &state->association_table[i];
        }
    }
    return NULL;  /* No empty slot available */
}

/**
 * Initialize learning system
 */
portia_learning_state_t* portia_learning_init(const portia_learning_config_t* config) {
    bbb_validation_result_t result = {0};

    // Validate config pointer
    if (config && !bbb_validate_pointer(NULL, config, sizeof(portia_learning_config_t), &result)) {
        LOG_ERROR("Invalid learning configuration pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_init: bbb_validate_pointer is NULL");
        return NULL;
    }

    LOG_INFO("Initializing Portia learning system");

    // Allocate state
    portia_learning_state_t* state = (portia_learning_state_t*)nimcp_calloc(
        1, sizeof(portia_learning_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate learning state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_learning_init: failed to allocate state");
        return NULL;
    }

    // Initialize configuration
    uint32_t hab_capacity = (config && config->max_habituation_entries > 0) ?
                           config->max_habituation_entries : DEFAULT_MAX_HABITUATION_ENTRIES;
    uint32_t assoc_capacity = (config && config->max_association_entries > 0) ?
                             config->max_association_entries : DEFAULT_MAX_ASSOCIATION_ENTRIES;

    // Validate capacities (reasonable limits)
    if (hab_capacity > 10000) {
        LOG_WARN("Habituation capacity %u exceeds limit, capping at 10000", hab_capacity);
        hab_capacity = 10000;
    }
    if (assoc_capacity > 10000) {
        LOG_WARN("Association capacity %u exceeds limit, capping at 10000", assoc_capacity);
        assoc_capacity = 10000;
    }

    // Allocate habituation table
    state->habituation_table = (habituation_entry_t*)nimcp_calloc(
        hab_capacity, sizeof(habituation_entry_t));
    if (!state->habituation_table) {
        LOG_ERROR("Failed to allocate habituation table");
        nimcp_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_learning_init: state->habituation_table is NULL");
        return NULL;
    }
    state->habituation_capacity = hab_capacity;

    // Allocate association table
    state->association_table = (association_entry_t*)nimcp_calloc(
        assoc_capacity, sizeof(association_entry_t));
    if (!state->association_table) {
        LOG_ERROR("Failed to allocate association table");
        nimcp_free(state->habituation_table);
        nimcp_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_learning_init: state->association_table is NULL");
        return NULL;
    }
    state->association_capacity = assoc_capacity;

    // Initialize learning parameters
    state->learning_rate = (config && config->default_learning_rate > 0) ?
                          config->default_learning_rate : DEFAULT_LEARNING_RATE;
    state->forgetting_rate = (config && config->default_forgetting_rate > 0) ?
                            config->default_forgetting_rate : DEFAULT_FORGETTING_RATE;
    state->consolidation_interval_ms = (config && config->consolidation_interval_ms > 0) ?
                                      config->consolidation_interval_ms :
                                      DEFAULT_CONSOLIDATION_INTERVAL_MS;

    state->active_mode = config ? config->allowed_modes : LEARNING_MODE_FULL;
    state->last_consolidation_ms = nimcp_time_monotonic_ms();

    // Create mutex
    state->mutex = (nimcp_platform_mutex_t*)nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!state->mutex) {
        LOG_ERROR("Failed to allocate learning mutex");
        nimcp_free(state->association_table);
        nimcp_free(state->habituation_table);
        nimcp_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_learning_init: state->mutex is NULL");
        return NULL;
    }
    if (nimcp_platform_mutex_init(state->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize learning mutex");
        nimcp_free(state->mutex);
        nimcp_free(state->association_table);
        nimcp_free(state->habituation_table);
        nimcp_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_init: validation failed");
        return NULL;
    }

    // Note: Bio-async inbox will be set later if needed
    state->inbox = NULL;

    // Register with bio-async router
    state->bio_ctx = NULL;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_PORTIA_LEARNING,
            .module_name = "portia_learning",
            .inbox_capacity = 16,
            .user_data = state
        };
        state->bio_ctx = bio_router_register_module(&bio_info);
        if (state->bio_ctx) {
            LOG_DEBUG("Registered with bio-async router");
        }
    }

    state->is_initialized = true;

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "init",
                  "hab=%u assoc=%u", hab_capacity, assoc_capacity);

    LOG_INFO("Portia learning initialized: hab=%u, assoc=%u, lr=%.3f, fr=%.3f",
             hab_capacity, assoc_capacity, state->learning_rate, state->forgetting_rate);

    return state;
}

/**
 * Destroy learning system
 */
void portia_learning_destroy(portia_learning_state_t* state) {
    if (!state) {
        return;
    }

    LOG_INFO("Destroying Portia learning system");

    // Unregister from bio-async router
    if (state->bio_ctx && bio_router_is_initialized()) {
        bio_router_unregister_module(state->bio_ctx);
        state->bio_ctx = NULL;
    }

    if (state->mutex) {
        nimcp_platform_mutex_lock(state->mutex);
    }

    if (state->association_table) {
        nimcp_free(state->association_table);
    }

    if (state->habituation_table) {
        nimcp_free(state->habituation_table);
    }

    if (state->mutex) {
        // Store mutex pointer before clearing state
        nimcp_platform_mutex_t* mutex = state->mutex;
        state->mutex = NULL;  // Prevent other threads from using it

        nimcp_platform_mutex_unlock(mutex);
        nimcp_platform_mutex_destroy(mutex);
        nimcp_free(mutex);
    }

    nimcp_free(state);

    LOG_DEBUG("Portia learning system destroyed");
}

/**
 * Set active learning mode
 */
int portia_learning_set_mode(portia_learning_state_t* state, portia_learning_mode_t mode) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_set_mode: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_set_mode: not initialized");
        return -1;
    }

    nimcp_platform_mutex_lock(state->mutex);

    portia_learning_mode_t old_mode = state->active_mode;
    state->active_mode = mode;

    nimcp_platform_mutex_unlock(state->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "LEARNING_MODE_CHANGE", "Changed learning mode from %d to %d",
                  old_mode, mode);

    LOG_INFO("Learning mode changed: %d -> %d", old_mode, mode);

    return 0;
}

/**
 * Process repeated stimulus (habituation)
 */
int portia_learning_habituate(portia_learning_state_t* state, uint32_t stimulus_id,
                               uint64_t timestamp_ms) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_set_mode: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_habituate: not initialized");
        return -1;
    }

    // Check if habituation is enabled
    if (!(state->active_mode & LEARNING_MODE_HABITUATION)) {
        LOG_DEBUG("Habituation mode not active");
        return 0;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Find existing entry
    habituation_entry_t* entry = find_habituation_entry(state, stimulus_id);

    if (entry) {
        // Apply spontaneous recovery if enough time has passed
        uint64_t time_since_exposure = timestamp_ms - entry->last_exposure_ms;
        if (time_since_exposure > RECOVERY_TIME_MS) {
            float recovery = entry->recovery_rate *
                           ((float)time_since_exposure / RECOVERY_TIME_MS);
            entry->response_strength = fminf(1.0F, entry->response_strength + recovery);
            LOG_DEBUG("Spontaneous recovery: stimulus=%u, strength=%.3f",
                     stimulus_id, entry->response_strength);
        }

        // Decrease response strength (habituation)
        float decrease = state->learning_rate * entry->habituation_rate;
        entry->response_strength *= (1.0F - decrease);
        entry->exposure_count++;
        entry->last_exposure_ms = timestamp_ms;

        LOG_DEBUG("Habituation: stimulus=%u, strength=%.3f, count=%u",
                 stimulus_id, entry->response_strength, entry->exposure_count);

    } else {
        // Create new entry
        entry = find_empty_habituation_slot(state);

        if (!entry) {
            // Table full, evict LRU entry
            entry = find_lru_habituation_entry(state);
            if (!entry) {
                nimcp_platform_mutex_unlock(state->mutex);
                LOG_ERROR("No habituation entry available for eviction");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "portia_learning_habituate: no entry available");
                return -1;
            }
            LOG_DEBUG("Evicting habituation entry: stimulus=%u", entry->stimulus_id);
            state->habituation_evictions++;
        }

        // Initialize new entry
        entry->stimulus_id = stimulus_id;
        entry->response_strength = 1.0F;  // Start at full strength
        entry->exposure_count = 1;
        entry->last_exposure_ms = timestamp_ms;
        entry->habituation_rate = HABITUATION_DECAY_FACTOR;
        entry->recovery_rate = 1.0F - HABITUATION_DECAY_FACTOR;
        entry->is_active = true;

        // Apply initial habituation decay (first exposure causes some habituation)
        float decrease = state->learning_rate * entry->habituation_rate;
        entry->response_strength *= (1.0F - decrease);

        state->habituation_count++;

        LOG_DEBUG("New habituation entry: stimulus=%u, strength=%.3f",
                 stimulus_id, entry->response_strength);
    }

    nimcp_platform_mutex_unlock(state->mutex);
    return 0;
}

/**
 * Increase response to important stimulus (sensitization)
 */
int portia_learning_sensitize(portia_learning_state_t* state, uint32_t stimulus_id,
                               float boost_amount, uint64_t timestamp_ms) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_set_mode: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_sensitize: not initialized");
        return -1;
    }

    // Check if sensitization is enabled
    if (!(state->active_mode & LEARNING_MODE_SENSITIZATION)) {
        LOG_DEBUG("Sensitization mode not active");
        return 0;
    }

    // Validate boost amount
    if (boost_amount < 0 || boost_amount > SENSITIZATION_BOOST_MAX) {
        LOG_WARN("Boost amount %.3f out of range, clamping", boost_amount);
        boost_amount = fmaxf(0.0F, fminf(SENSITIZATION_BOOST_MAX, boost_amount));
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Find or create habituation entry
    habituation_entry_t* entry = find_habituation_entry(state, stimulus_id);

    if (!entry) {
        entry = find_empty_habituation_slot(state);
        if (!entry) {
            entry = find_lru_habituation_entry(state);
        }

        if (entry) {
            entry->stimulus_id = stimulus_id;
            entry->response_strength = 1.0F;
            entry->exposure_count = 0;
            entry->last_exposure_ms = timestamp_ms;
            entry->habituation_rate = HABITUATION_DECAY_FACTOR;
            entry->recovery_rate = 1.0F - HABITUATION_DECAY_FACTOR;
            entry->is_active = true;
            state->habituation_count++;
        }
    }

    if (entry) {
        // Boost response strength (sensitization)
        entry->response_strength = fminf(SENSITIZATION_BOOST_MAX,
                                        entry->response_strength + boost_amount);
        entry->last_exposure_ms = timestamp_ms;

        LOG_DEBUG("Sensitization: stimulus=%u, strength=%.3f, boost=%.3f",
                 stimulus_id, entry->response_strength, boost_amount);
    }

    nimcp_platform_mutex_unlock(state->mutex);
    return 0;
}

/**
 * Create or strengthen association
 */
int portia_learning_associate(portia_learning_state_t* state, uint32_t stimulus_id,
                               uint32_t response_id, bool is_positive,
                               uint64_t timestamp_ms) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_set_mode: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_associate: not initialized");
        return -1;
    }

    // Check if associative learning is enabled
    if (!(state->active_mode & LEARNING_MODE_ASSOCIATIVE)) {
        LOG_DEBUG("Associative learning mode not active");
        return 0;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Find existing association
    association_entry_t* entry = find_association_entry(state, stimulus_id, response_id);

    if (entry) {
        // Strengthen existing association
        entry->association_strength = fminf(1.0F,
            entry->association_strength + state->learning_rate);
        entry->reinforcement_count++;
        entry->last_reinforcement_ms = timestamp_ms;
        entry->is_positive = is_positive;  // Update valence

        LOG_DEBUG("Strengthened association: stimulus=%u, response=%u, strength=%.3f, count=%u",
                 stimulus_id, response_id, entry->association_strength,
                 entry->reinforcement_count);

    } else {
        // Create new association
        entry = find_empty_association_slot(state);

        if (!entry) {
            // Table full, evict LRU entry
            entry = find_lru_association_entry(state);
            if (!entry) {
                nimcp_platform_mutex_unlock(state->mutex);
                LOG_ERROR("No association entry available for eviction");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "portia_learning_associate: no entry available");
                return -1;
            }
            LOG_DEBUG("Evicting association entry: stimulus=%u, response=%u",
                     entry->stimulus_id, entry->response_id);
            state->association_evictions++;
        }

        // Initialize new association
        entry->stimulus_id = stimulus_id;
        entry->response_id = response_id;
        entry->association_strength = fminf(1.0f, state->learning_rate);  // Initial strength clamped
        entry->reinforcement_count = 1;
        entry->last_reinforcement_ms = timestamp_ms;
        entry->is_positive = is_positive;
        entry->is_active = true;

        state->association_count++;

        LOG_DEBUG("New association: stimulus=%u, response=%u, positive=%d",
                 stimulus_id, response_id, is_positive);
    }

    nimcp_platform_mutex_unlock(state->mutex);
    return 0;
}

/**
 * Reinforce association (learning from outcome)
 */
int portia_learning_reinforce(portia_learning_state_t* state, uint32_t stimulus_id,
                               uint32_t response_id, float reward, uint64_t timestamp_ms) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_reinforce: not initialized");
        return -1;
    }

    // Check if trial-and-error learning is enabled
    if (!(state->active_mode & LEARNING_MODE_TRIAL_ERROR)) {
        LOG_DEBUG("Trial-and-error mode not active");
        return 0;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Find association
    association_entry_t* entry = find_association_entry(state, stimulus_id, response_id);

    if (!entry) {
        // Create association if it doesn't exist
        entry = find_empty_association_slot(state);
        if (!entry) {
            entry = find_lru_association_entry(state);
        }

        if (entry) {
            entry->stimulus_id = stimulus_id;
            entry->response_id = response_id;
            entry->association_strength = 0.5F;  // Neutral start
            entry->reinforcement_count = 0;
            entry->last_reinforcement_ms = timestamp_ms;
            entry->is_positive = (reward > 0);
            entry->is_active = true;
            state->association_count++;
        } else {
            nimcp_platform_mutex_unlock(state->mutex);
            LOG_ERROR("No association entry available");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "portia_learning_reinforce: no entry available");
            return -1;
        }
    }

    // Apply reinforcement learning
    // Positive reward strengthens, negative weakens
    // Use reward directly (not scaled by learning_rate) for effective reinforcement
    float delta = reward;
    entry->association_strength = fmaxf(0.0F, fminf(1.0F,
                                       entry->association_strength + delta));
    entry->reinforcement_count++;
    entry->last_reinforcement_ms = timestamp_ms;

    LOG_DEBUG("Reinforcement: stimulus=%u, response=%u, reward=%.3f, strength=%.3f",
             stimulus_id, response_id, reward, entry->association_strength);

    nimcp_platform_mutex_unlock(state->mutex);
    return 0;
}

/**
 * Query learned response strength
 */
portia_learning_query_result_t portia_learning_query(portia_learning_state_t* state,
                                                      uint32_t stimulus_id) {
    portia_learning_query_result_t query_result = {0};
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        return query_result;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        return query_result;
    }

    nimcp_platform_mutex_lock(state->mutex);

    habituation_entry_t* entry = find_habituation_entry(state, stimulus_id);
    if (entry) {
        query_result.found = true;
        query_result.strength = entry->response_strength;
        query_result.exposure_count = entry->exposure_count;
        query_result.last_update_ms = entry->last_exposure_ms;
    }

    nimcp_platform_mutex_unlock(state->mutex);
    return query_result;
}

/**
 * Query association strength
 */
portia_learning_query_result_t portia_learning_query_association(
    portia_learning_state_t* state, uint32_t stimulus_id, uint32_t response_id) {

    portia_learning_query_result_t query_result = {0};
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        return query_result;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        return query_result;
    }

    nimcp_platform_mutex_lock(state->mutex);

    association_entry_t* entry = find_association_entry(state, stimulus_id, response_id);
    if (entry) {
        query_result.found = true;
        query_result.strength = entry->association_strength;
        query_result.exposure_count = entry->reinforcement_count;
        query_result.last_update_ms = entry->last_reinforcement_ms;
    }

    nimcp_platform_mutex_unlock(state->mutex);
    return query_result;
}

/**
 * Apply forgetting to all entries
 */
int portia_learning_forget(portia_learning_state_t* state, uint64_t timestamp_ms) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_forget: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_forget: not initialized");
        return -1;
    }

    nimcp_platform_mutex_lock(state->mutex);

    uint32_t forgotten_count = 0;

    // Apply forgetting to habituation entries
    for (uint32_t i = 0; i < state->habituation_capacity; i++) {
        if (state->habituation_table[i].is_active) {
            habituation_entry_t* entry = &state->habituation_table[i];

            // Natural decay over time
            uint64_t time_since = timestamp_ms - entry->last_exposure_ms;
            float decay = state->forgetting_rate * ((float)time_since / 1000.0F);

            // Decay toward baseline (1.0)
            if (entry->response_strength < 1.0F) {
                entry->response_strength += decay;
                if (entry->response_strength >= 1.0F) {
                    entry->response_strength = 1.0F;
                }
            } else if (entry->response_strength > 1.0F) {
                entry->response_strength -= decay;
                if (entry->response_strength <= 1.0F) {
                    entry->response_strength = 1.0F;
                }
            }
        }
    }

    // Apply forgetting to associations
    for (uint32_t i = 0; i < state->association_capacity; i++) {
        if (state->association_table[i].is_active) {
            association_entry_t* entry = &state->association_table[i];

            // Decay association strength
            entry->association_strength *= (1.0F - state->forgetting_rate);

            // Remove if too weak
            if (entry->association_strength < DEFAULT_ASSOCIATION_THRESHOLD) {
                entry->is_active = false;
                state->association_count--;
                forgotten_count++;
            }
        }
    }

    nimcp_platform_mutex_unlock(state->mutex);

    if (forgotten_count > 0) {
        LOG_DEBUG("Forgetting applied: %u associations removed", forgotten_count);
    }

    return 0;
}

/**
 * Consolidate memories (strengthen important, remove weak)
 */
int portia_learning_consolidate(portia_learning_state_t* state, uint64_t timestamp_ms) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_consolidate: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_consolidate: not initialized");
        return -1;
    }

    // Check if enough time has passed since last consolidation
    if (timestamp_ms - state->last_consolidation_ms < state->consolidation_interval_ms) {
        return 0;
    }

    nimcp_platform_mutex_lock(state->mutex);

    uint32_t consolidated_count = 0;
    uint32_t removed_count = 0;

    // Consolidate habituation entries
    for (uint32_t i = 0; i < state->habituation_capacity; i++) {
        if (state->habituation_table[i].is_active) {
            habituation_entry_t* entry = &state->habituation_table[i];

            // Remove entries that haven't been accessed recently and are weak
            uint64_t time_since = timestamp_ms - entry->last_exposure_ms;
            if (time_since > RECOVERY_TIME_MS * 2 &&
                entry->response_strength < DEFAULT_HABITUATION_THRESHOLD) {
                entry->is_active = false;
                state->habituation_count--;
                removed_count++;
            }
            // Strengthen frequently accessed entries
            else if (entry->exposure_count > 10) {
                entry->habituation_rate *= 0.95F;  // Slower habituation
                consolidated_count++;
            }
        }
    }

    // Consolidate associations
    for (uint32_t i = 0; i < state->association_capacity; i++) {
        if (state->association_table[i].is_active) {
            association_entry_t* entry = &state->association_table[i];

            // Remove weak associations
            if (entry->association_strength < DEFAULT_ASSOCIATION_THRESHOLD) {
                entry->is_active = false;
                state->association_count--;
                removed_count++;
            }
            // Strengthen well-reinforced associations
            else if (entry->reinforcement_count > 5) {
                entry->association_strength = fminf(1.0F,
                    entry->association_strength * 1.05F);
                consolidated_count++;
            }
        }
    }

    state->last_consolidation_ms = timestamp_ms;

    nimcp_platform_mutex_unlock(state->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "consolidation",
                  "consolidated=%u removed=%u", consolidated_count, removed_count);

    LOG_INFO("Memory consolidation: strengthened=%u, removed=%u",
             consolidated_count, removed_count);

    return 0;
}

/**
 * Get learning statistics
 */
portia_learning_stats_t portia_learning_get_stats(portia_learning_state_t* state) {
    portia_learning_stats_t stats = {0};
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        return stats;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        return stats;
    }

    nimcp_platform_mutex_lock(state->mutex);

    float total_hab_strength = 0.0F;
    float total_assoc_strength = 0.0F;

    // Count active entries and calculate averages
    for (uint32_t i = 0; i < state->habituation_capacity; i++) {
        if (state->habituation_table[i].is_active) {
            stats.active_habituation_entries++;
            total_hab_strength += state->habituation_table[i].response_strength;
            stats.total_exposures += state->habituation_table[i].exposure_count;
        }
    }

    for (uint32_t i = 0; i < state->association_capacity; i++) {
        if (state->association_table[i].is_active) {
            stats.active_association_entries++;
            total_assoc_strength += state->association_table[i].association_strength;
            stats.total_reinforcements += state->association_table[i].reinforcement_count;
        }
    }

    stats.total_habituation_entries = state->habituation_capacity;
    stats.total_association_entries = state->association_capacity;
    stats.habituation_evictions = state->habituation_evictions;
    stats.association_evictions = state->association_evictions;

    if (stats.active_habituation_entries > 0) {
        stats.avg_habituation_strength = total_hab_strength / stats.active_habituation_entries;
    }

    if (stats.active_association_entries > 0) {
        stats.avg_association_strength = total_assoc_strength / stats.active_association_entries;
    }

    nimcp_platform_mutex_unlock(state->mutex);

    return stats;
}

/**
 * Reset all learning
 */
int portia_learning_reset(portia_learning_state_t* state) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_reset: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_reset: not initialized");
        return -1;
    }

    nimcp_platform_mutex_lock(state->mutex);

    // Clear all entries
    memset(state->habituation_table, 0,
           state->habituation_capacity * sizeof(habituation_entry_t));
    memset(state->association_table, 0,
           state->association_capacity * sizeof(association_entry_t));

    state->habituation_count = 0;
    state->association_count = 0;
    state->habituation_evictions = 0;
    state->association_evictions = 0;

    nimcp_platform_mutex_unlock(state->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "reset", "learning data cleared");
    LOG_INFO("Learning system reset");

    return 0;
}

/**
 * Export learning data (for inspection/debugging)
 */
int portia_learning_export(portia_learning_state_t* state, const char* filepath) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_export: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!bbb_validate_pointer(NULL, filepath, 1, &result)) {
        LOG_ERROR("Invalid filepath pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_export: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        LOG_ERROR("Learning system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_learning_export: not initialized");
        return -1;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        LOG_ERROR("Failed to open export file: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "portia_learning_export: failed to open file");
        return -1;
    }

    nimcp_platform_mutex_lock(state->mutex);

    fprintf(fp, "=== Portia Learning System Export ===\n\n");
    fprintf(fp, "Mode: %d\n", state->active_mode);
    fprintf(fp, "Learning Rate: %.4f\n", state->learning_rate);
    fprintf(fp, "Forgetting Rate: %.4f\n\n", state->forgetting_rate);

    fprintf(fp, "=== Habituation Entries (%u/%u) ===\n",
            state->habituation_count, state->habituation_capacity);
    for (uint32_t i = 0; i < state->habituation_capacity; i++) {
        if (state->habituation_table[i].is_active) {
            habituation_entry_t* e = &state->habituation_table[i];
            fprintf(fp, "Stimulus %u: strength=%.3f, count=%u, last=%llu\n",
                   e->stimulus_id, e->response_strength, e->exposure_count,
                   (unsigned long long)e->last_exposure_ms);
        }
    }

    fprintf(fp, "\n=== Association Entries (%u/%u) ===\n",
            state->association_count, state->association_capacity);
    for (uint32_t i = 0; i < state->association_capacity; i++) {
        if (state->association_table[i].is_active) {
            association_entry_t* e = &state->association_table[i];
            fprintf(fp, "S%u->R%u: strength=%.3f, count=%u, positive=%d, last=%llu\n",
                   e->stimulus_id, e->response_id, e->association_strength,
                   e->reinforcement_count, e->is_positive,
                   (unsigned long long)e->last_reinforcement_ms);
        }
    }

    nimcp_platform_mutex_unlock(state->mutex);

    fclose(fp);

    LOG_INFO("Learning data exported to: %s", filepath);
    return 0;
}

/**
 * Process bio-async messages
 */
int portia_learning_process_inbox(portia_learning_state_t* state) {
    bbb_validation_result_t result = {0};

    if (!bbb_validate_pointer(NULL, state, sizeof(portia_learning_state_t), &result)) {
        LOG_ERROR("Invalid learning state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_learning_process_inbox: bbb_validate_pointer is NULL");
        return -1;
    }

    if (!state->is_initialized) {
        return 0;
    }

    // Process messages from bio-async router
    if (state->bio_ctx && bio_router_is_initialized()) {
        uint32_t processed = bio_router_process_inbox(state->bio_ctx, 5);
        if (processed > 0) {
            LOG_DEBUG("Processed %u bio-async messages", processed);
        }
        return (int)processed;
    }

    return 0;
}
