//=============================================================================
// nimcp_spaced_repetition.c - Spaced Repetition System Implementation
//=============================================================================
/**
 * @file nimcp_spaced_repetition.c
 * @brief Implementation of optimized learning schedules based on forgetting curves
 *
 * Implements the FSRS (Free Spaced Repetition Scheduler) algorithm integrated
 * with Prime Resonant memory architecture for biologically-inspired learning.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_spaced_repetition.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <float.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(spaced_repetition, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Thread-Local Error State
//=============================================================================

#ifdef _WIN32
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char sr_error_buffer[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void sr_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(sr_error_buffer, sizeof(sr_error_buffer), fmt, args);
    va_end(args);
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Priority queue node for review scheduling
 */
typedef struct sr_heap_node {
    sr_review_queue_entry_t entry;
    size_t heap_index;              /**< Index in heap array */
} sr_heap_node_t;

/**
 * @brief Hash table entry for item lookup
 */
typedef struct sr_hash_entry {
    uint64_t item_id;
    sr_spaced_item_t* item;
    struct sr_hash_entry* next;
} sr_hash_entry_t;

/**
 * @brief Internal spaced repetition system structure
 */
struct sr_system_struct {
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    sr_config_t config;

    //-------------------------------------------------------------------------
    // Items (hash table for O(1) lookup)
    //-------------------------------------------------------------------------
    sr_hash_entry_t** items_table;
    size_t table_size;
    size_t num_items;

    //-------------------------------------------------------------------------
    // Review Queue (min-heap by due_time)
    //-------------------------------------------------------------------------
    sr_heap_node_t** heap;
    size_t heap_size;
    size_t heap_capacity;

    //-------------------------------------------------------------------------
    // ID Generation
    //-------------------------------------------------------------------------
    uint64_t next_item_id;

    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    sr_stats_t stats;
    uint64_t session_start_ms;
    size_t reviews_this_session;

    //-------------------------------------------------------------------------
    // Daily Tracking
    //-------------------------------------------------------------------------
    uint64_t day_start_ms;
    size_t new_today;
    size_t reviews_today;

    //-------------------------------------------------------------------------
    // History (for retention calculation)
    //-------------------------------------------------------------------------
    size_t* daily_review_counts;
    float* daily_retention_rates;
    size_t history_days;
    size_t history_capacity;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void heap_sift_up(sr_system_t system, size_t index);
static void heap_sift_down(sr_system_t system, size_t index);
static void heap_insert(sr_system_t system, sr_spaced_item_t* item);
static sr_spaced_item_t* heap_extract_min(sr_system_t system);
static void heap_update(sr_system_t system, sr_spaced_item_t* item);
static void heap_remove(sr_system_t system, sr_spaced_item_t* item);

static uint64_t hash_id(uint64_t id, size_t table_size);
static sr_spaced_item_t* hash_lookup(sr_system_t system, uint64_t item_id);
static void hash_insert(sr_system_t system, sr_spaced_item_t* item);
static void hash_remove(sr_system_t system, uint64_t item_id);

static sr_spaced_item_t* item_create(sr_system_t system, pr_memory_node_t* memory);
static void item_destroy(sr_spaced_item_t* item);
static void item_add_history(sr_spaced_item_t* item, const sr_review_record_t* record);

static float compute_stability_increase(
    sr_system_t system,
    const sr_spaced_item_t* item,
    sr_review_response_t response
);
static float compute_new_difficulty(
    const sr_spaced_item_t* item,
    sr_review_response_t response
);
static float add_interval_fuzz(float interval);
static void update_daily_stats(sr_system_t system);
static float compute_entry_priority(sr_system_t system, const sr_spaced_item_t* item);

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT sr_config_t sr_config_default(void) {
    sr_config_t config;
    memset(&config, 0, sizeof(config));

    // FSRS parameters
    config.fsrs_params = sr_fsrs_params_default();

    // Retention target
    config.target_retention = SR_DEFAULT_TARGET_RETENTION;
    config.max_interval_days = SR_DEFAULT_MAX_INTERVAL_DAYS;
    config.min_interval_days = SR_DEFAULT_MIN_INTERVAL_DAYS;

    // Learning steps (minutes): 1, 10, 60, 1440 (1 day)
    config.learning_steps_minutes[0] = 1.0f;
    config.learning_steps_minutes[1] = 10.0f;
    config.learning_steps_minutes[2] = 60.0f;
    config.learning_steps_minutes[3] = 1440.0f;
    config.num_learning_steps = 4;
    config.graduating_interval_days = 1.0f;
    config.easy_interval_days = 4.0f;

    // Relearning steps
    config.relearning_steps_minutes[0] = 10.0f;
    config.relearning_steps_minutes[1] = 60.0f;
    config.num_relearning_steps = 2;
    config.min_review_interval_days = 1.0f;

    // Modifiers
    config.easy_bonus = SR_EASY_BONUS;
    config.hard_penalty = SR_HARD_PENALTY;
    config.interval_modifier = 1.0f;
    config.new_interval_after_lapse = 0.0f;

    // Behavior
    config.fuzz_intervals = true;
    config.bury_siblings = false;
    config.max_reviews_per_day = 0;  // Unlimited
    config.max_new_per_day = 20;
    config.leech_threshold = SR_MAX_LAPSES_WARN;

    // Integration
    config.sync_with_z_ladder = true;
    config.use_entanglement_cues = true;
    config.history_capacity = SR_MAX_HISTORY_LENGTH;

    return config;
}

NIMCP_EXPORT sr_fsrs_params_t sr_fsrs_params_default(void) {
    sr_fsrs_params_t params;

    params.param_a = SR_DEFAULT_STABILITY_GROWTH_A;
    params.param_b = SR_DEFAULT_DIFFICULTY_SENSITIVITY;
    params.param_c = SR_DEFAULT_STABILITY_DECAY;
    params.param_d = SR_DEFAULT_RETRIEVABILITY_BONUS;
    params.initial_stability = SR_DEFAULT_INITIAL_STABILITY;
    params.lapse_stability_factor = SR_LAPSE_DECAY_FACTOR;
    params.lapse_decay_exponent = 0.2f;

    return params;
}

NIMCP_EXPORT bool sr_config_validate(const sr_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_config_validate: config is NULL");
        return false;
    }

    // Validate retention target
    if (config->target_retention <= 0.0f || config->target_retention >= 1.0f) {
        sr_set_error("Invalid target_retention: must be in (0, 1)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sr_config_validate: capacity exceeded");
        return false;
    }

    // Validate intervals
    if (config->max_interval_days <= 0.0f) {
        sr_set_error("Invalid max_interval_days: must be positive");
        return false;
    }
    if (config->min_interval_days < 0.0f) {
        sr_set_error("Invalid min_interval_days: must be non-negative");
        return false;
    }
    if (config->min_interval_days >= config->max_interval_days) {
        sr_set_error("min_interval_days must be less than max_interval_days");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "sr_config_validate: capacity exceeded");
        return false;
    }

    // Validate FSRS parameters
    const sr_fsrs_params_t* p = &config->fsrs_params;
    if (p->param_a <= 0.0f || p->param_b < 0.0f || p->param_c < 0.0f || p->param_d < 0.0f) {
        sr_set_error("Invalid FSRS parameters");
        return false;
    }
    if (p->initial_stability <= 0.0f) {
        sr_set_error("Invalid initial_stability: must be positive");
        return false;
    }

    // Validate modifiers
    if (config->easy_bonus < 1.0f) {
        sr_set_error("Invalid easy_bonus: must be >= 1.0");
        return false;
    }
    if (config->hard_penalty <= 0.0f || config->hard_penalty > 1.0f) {
        sr_set_error("Invalid hard_penalty: must be in (0, 1]");
        return false;
    }
    if (config->interval_modifier <= 0.0f) {
        sr_set_error("Invalid interval_modifier: must be positive");
        return false;
    }

    return true;
}

NIMCP_EXPORT sr_config_t sr_config_high_retention(void) {
    sr_config_t config = sr_config_default();
    config.target_retention = 0.95f;
    config.interval_modifier = 0.8f;
    config.max_new_per_day = 10;
    return config;
}

NIMCP_EXPORT sr_config_t sr_config_efficient(void) {
    sr_config_t config = sr_config_default();
    config.target_retention = 0.85f;
    config.interval_modifier = 1.2f;
    config.easy_bonus = 1.5f;
    config.max_new_per_day = 30;
    return config;
}

//=============================================================================
// System Lifecycle Functions
//=============================================================================

NIMCP_EXPORT sr_system_t sr_system_create(const sr_config_t* config) {
    return sr_system_create_integrated(config, NULL, NULL);
}

NIMCP_EXPORT sr_system_t sr_system_create_integrated(
    const sr_config_t* config,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager
) {
    sr_config_t cfg;
    if (config) {
        cfg = *config;
        if (!sr_config_validate(&cfg)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_system_create_integrated: sr_config_validate is NULL");
            return NULL;
        }
    } else {
        cfg = sr_config_default();
    }

    sr_system_t system = (sr_system_t)nimcp_calloc(1, sizeof(struct sr_system_struct));
    if (!system) {
        sr_set_error("Failed to allocate system structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sr_system_create_integrated: system is NULL");
        return NULL;
    }

    system->config = cfg;
    system->entanglement = entanglement;
    system->node_manager = node_manager;

    // Initialize hash table
    system->table_size = 1024;
    system->items_table = (sr_hash_entry_t**)nimcp_calloc(system->table_size, sizeof(sr_hash_entry_t*));
    if (!system->items_table) {
        sr_set_error("Failed to allocate hash table");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sr_system_create_integrated: system->items_table is NULL");
        return NULL;
    }

    // Initialize heap
    system->heap_capacity = SR_DEFAULT_QUEUE_CAPACITY;
    system->heap = (sr_heap_node_t**)nimcp_calloc(system->heap_capacity, sizeof(sr_heap_node_t*));
    if (!system->heap) {
        sr_set_error("Failed to allocate heap");
        nimcp_free(system->items_table);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sr_system_create_integrated: system->heap is NULL");
        return NULL;
    }

    // Initialize history tracking
    system->history_capacity = 365;
    system->daily_review_counts = (size_t*)nimcp_calloc(system->history_capacity, sizeof(size_t));
    system->daily_retention_rates = (float*)nimcp_calloc(system->history_capacity, sizeof(float));
    if (!system->daily_review_counts || !system->daily_retention_rates) {
        sr_set_error("Failed to allocate history arrays");
        nimcp_free(system->daily_retention_rates);
        nimcp_free(system->daily_review_counts);
        nimcp_free(system->heap);
        nimcp_free(system->items_table);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_system_create_integrated: required parameter is NULL (system->daily_review_counts, system->daily_retention_rates)");
        return NULL;
    }

    // Initialize time tracking
    system->next_item_id = 1;
    system->session_start_ms = sr_current_time_ms();
    system->day_start_ms = (system->session_start_ms / 86400000ULL) * 86400000ULL;

    return system;
}

NIMCP_EXPORT void sr_system_destroy(sr_system_t system) {
    if (!system) {
        return;
    }

    // Free all items via hash table
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            sr_hash_entry_t* next = entry->next;
            item_destroy(entry->item);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }

    // Free heap nodes (items already freed)
    for (size_t i = 0; i < system->heap_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->heap_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->heap_size);
        }

        nimcp_free(system->heap[i]);
    }

    nimcp_free(system->heap);
    nimcp_free(system->items_table);
    nimcp_free(system->daily_review_counts);
    nimcp_free(system->daily_retention_rates);
    nimcp_free(system);
    system = NULL;
}

NIMCP_EXPORT sr_error_t sr_system_clear(sr_system_t system) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    // Free all items
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            sr_hash_entry_t* next = entry->next;
            item_destroy(entry->item);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        system->items_table[i] = NULL;
    }

    // Clear heap
    for (size_t i = 0; i < system->heap_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->heap_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->heap_size);
        }

        nimcp_free(system->heap[i]);
        system->heap[i] = NULL;
    }
    system->heap_size = 0;
    system->num_items = 0;

    // Reset statistics
    memset(&system->stats, 0, sizeof(sr_stats_t));

    return SR_SUCCESS;
}

NIMCP_EXPORT const sr_config_t* sr_system_get_config(sr_system_t system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }
    return &system->config;
}

NIMCP_EXPORT sr_error_t sr_system_set_config(sr_system_t system, const sr_config_t* config) {
    if (!system || !config) {
        return SR_ERROR_NULL_POINTER;
    }
    if (!sr_config_validate(config)) {
        return SR_ERROR_INVALID_CONFIG;
    }
    system->config = *config;
    return SR_SUCCESS;
}

//=============================================================================
// Item Management Functions
//=============================================================================

NIMCP_EXPORT uint64_t sr_system_add_item(sr_system_t system, pr_memory_node_t* memory) {
    return sr_system_add_item_with_difficulty(system, memory, SR_DEFAULT_DIFFICULTY);
}

NIMCP_EXPORT uint64_t sr_system_add_item_with_difficulty(
    sr_system_t system,
    pr_memory_node_t* memory,
    float initial_difficulty
) {
    if (!system || !memory) {
        sr_set_error("NULL system or memory");
        return 0;
    }

    // Check if memory already tracked
    if (sr_system_get_item_by_memory(system, memory) != NULL) {
        sr_set_error("Memory already in system");
        return 0;
    }

    // Create new item
    sr_spaced_item_t* item = item_create(system, memory);
    if (!item) {
        return 0;
    }

    // Set initial difficulty
    if (initial_difficulty < SR_MIN_DIFFICULTY) {
        initial_difficulty = SR_MIN_DIFFICULTY;
    }
    if (initial_difficulty > SR_MAX_DIFFICULTY) {
        initial_difficulty = SR_MAX_DIFFICULTY;
    }
    item->strength.difficulty = initial_difficulty;

    // Add to hash table
    hash_insert(system, item);

    // Add to heap (new items are immediately due)
    heap_insert(system, item);

    // Update stats
    system->stats.total_items++;
    system->stats.new_items++;

    return item->item_id;
}

NIMCP_EXPORT sr_error_t sr_system_remove_item(sr_system_t system, uint64_t item_id) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    // Update stats based on state
    switch (item->state) {
        case SR_STATE_NEW:
            system->stats.new_items--;
            break;
        case SR_STATE_LEARNING:
            system->stats.learning_items--;
            break;
        case SR_STATE_REVIEW:
            system->stats.review_items--;
            break;
        case SR_STATE_RELEARNING:
            system->stats.relearning_items--;
            break;
        case SR_STATE_SUSPENDED:
            system->stats.suspended_items--;
            break;
        default:
            break;
    }
    system->stats.total_items--;

    if (item->is_leech) {
        system->stats.leech_items--;
    }

    // Remove from heap
    heap_remove(system, item);

    // Remove from hash table
    hash_remove(system, item_id);

    // Destroy item
    item_destroy(item);

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_spaced_item_t* sr_system_get_item(sr_system_t system, uint64_t item_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }
    return hash_lookup(system, item_id);
}

NIMCP_EXPORT sr_spaced_item_t* sr_system_get_item_by_memory(
    sr_system_t system,
    const pr_memory_node_t* memory
) {
    if (!system || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sr_system_get_item_by_memory: required parameter is NULL (system, memory)");
        return NULL;
    }

    uint64_t memory_id = pr_memory_node_get_id(memory);

    // Linear scan (could optimize with secondary index)
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            if (entry->item->memory == memory ||
                pr_memory_node_get_id(entry->item->memory) == memory_id) {
                return entry->item;
            }
            entry = entry->next;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_system_get_item_by_memory: operation failed");
    return NULL;
}

NIMCP_EXPORT size_t sr_system_get_item_count(sr_system_t system) {
    if (!system) {
        return 0;
    }
    return system->num_items;
}

NIMCP_EXPORT sr_error_t sr_system_suspend_item(sr_system_t system, uint64_t item_id) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    if (item->is_suspended) {
        return SR_SUCCESS;  // Already suspended
    }

    // Update stats
    switch (item->state) {
        case SR_STATE_NEW:
            system->stats.new_items--;
            break;
        case SR_STATE_LEARNING:
            system->stats.learning_items--;
            break;
        case SR_STATE_REVIEW:
            system->stats.review_items--;
            break;
        case SR_STATE_RELEARNING:
            system->stats.relearning_items--;
            break;
        default:
            break;
    }
    system->stats.suspended_items++;

    item->is_suspended = true;
    item->state = SR_STATE_SUSPENDED;

    // Remove from heap
    heap_remove(system, item);

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_system_unsuspend_item(sr_system_t system, uint64_t item_id) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    if (!item->is_suspended) {
        return SR_SUCCESS;  // Not suspended
    }

    item->is_suspended = false;
    system->stats.suspended_items--;

    // Determine appropriate state
    if (item->repetition_count == 0) {
        item->state = SR_STATE_NEW;
        system->stats.new_items++;
    } else {
        item->state = SR_STATE_REVIEW;
        system->stats.review_items++;
    }

    // Re-add to heap
    heap_insert(system, item);

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_system_bury_item(sr_system_t system, uint64_t item_id) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    item->state = SR_STATE_BURIED;
    heap_remove(system, item);

    return SR_SUCCESS;
}

NIMCP_EXPORT size_t sr_system_unbury_all(sr_system_t system) {
    if (!system) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            if (entry->item->state == SR_STATE_BURIED) {
                entry->item->state = (entry->item->repetition_count == 0)
                    ? SR_STATE_NEW : SR_STATE_REVIEW;
                heap_insert(system, entry->item);
                count++;
            }
            entry = entry->next;
        }
    }

    return count;
}

//=============================================================================
// Review Functions
//=============================================================================

NIMCP_EXPORT sr_spaced_item_t* sr_system_get_next(sr_system_t system) {
    if (!system || system->heap_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sr_system_get_next: system is NULL");
        return NULL;
    }

    float current_time = sr_current_time_days();

    // Check if top item is due
    sr_spaced_item_t* top = system->heap[0]->entry.item;
    if (top->due_time > current_time) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_system_get_next: validation failed");
        return NULL;  // Nothing due yet
    }

    return top;
}

NIMCP_EXPORT sr_spaced_item_t* sr_system_peek_next(sr_system_t system) {
    return sr_system_get_next(system);  // Same behavior for now
}

NIMCP_EXPORT sr_error_t sr_system_review(
    sr_system_t system,
    uint64_t item_id,
    sr_review_response_t response,
    float response_time_ms
) {
    return sr_system_review_at_time(system, item_id, response, response_time_ms,
                                     sr_current_time_ms());
}

NIMCP_EXPORT sr_error_t sr_system_review_at_time(
    sr_system_t system,
    uint64_t item_id,
    sr_review_response_t response,
    float response_time_ms,
    uint64_t timestamp_ms
) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }
    if (response < SR_RESPONSE_AGAIN || response > SR_RESPONSE_EASY) {
        return SR_ERROR_INVALID_RESPONSE;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }
    if (item->is_suspended) {
        return SR_ERROR_SUSPENDED;
    }

    float current_time_days = sr_ms_to_days(timestamp_ms);

    // Calculate retrievability at review time
    float time_elapsed = current_time_days - item->last_review_time;
    if (time_elapsed < 0) {
        time_elapsed = 0;
    }
    float retrievability = sr_predict_retention(item, time_elapsed);

    // Store values before update
    float stability_before = item->strength.current_stability;
    sr_item_state_t old_state = item->state;

    // Process based on response
    float new_stability = 0.0f;
    float new_interval = 0.0f;
    sr_item_state_t new_state;

    if (response == SR_RESPONSE_AGAIN) {
        // Lapse - memory was forgotten
        item->lapses++;

        // Check for leech
        if (item->lapses >= system->config.leech_threshold && !item->is_leech) {
            item->is_leech = true;
            system->stats.leech_items++;
        }

        // Reset stability based on lapse count
        const sr_fsrs_params_t* p = &system->config.fsrs_params;
        new_stability = p->initial_stability *
            powf(item->strength.difficulty, -p->lapse_stability_factor) *
            powf((float)item->lapses + 1.0f, -p->lapse_decay_exponent);

        // Clamp minimum stability
        if (new_stability < system->config.min_interval_days) {
            new_stability = system->config.min_interval_days;
        }

        // Go to relearning
        new_state = SR_STATE_RELEARNING;
        new_interval = system->config.relearning_steps_minutes[0] / 1440.0f;  // Minutes to days

        // Update stats
        system->stats.again_count++;

    } else {
        // Successful recall
        new_stability = compute_stability_increase(system, item, response);
        item->repetition_count++;

        // Compute new interval
        new_interval = sr_interval_from_stability(new_stability, system->config.target_retention);

        // Apply modifiers
        new_interval *= system->config.interval_modifier;

        if (response == SR_RESPONSE_EASY) {
            new_interval *= system->config.easy_bonus;
            system->stats.easy_count++;
        } else if (response == SR_RESPONSE_HARD) {
            new_interval *= system->config.hard_penalty;
            system->stats.hard_count++;
        } else {
            system->stats.good_count++;
        }

        // Clamp interval
        if (new_interval < system->config.min_interval_days) {
            new_interval = system->config.min_interval_days;
        }
        if (new_interval > system->config.max_interval_days) {
            new_interval = system->config.max_interval_days;
        }

        // Add fuzz if enabled
        if (system->config.fuzz_intervals) {
            new_interval = add_interval_fuzz(new_interval);
        }

        // Determine new state
        if (item->state == SR_STATE_NEW || item->state == SR_STATE_LEARNING) {
            if (response == SR_RESPONSE_EASY || item->repetition_count >= system->config.num_learning_steps) {
                new_state = SR_STATE_REVIEW;  // Graduate
            } else {
                new_state = SR_STATE_LEARNING;
            }
        } else if (item->state == SR_STATE_RELEARNING) {
            new_state = SR_STATE_REVIEW;  // Graduate from relearning
        } else {
            new_state = SR_STATE_REVIEW;
        }
    }

    // Update difficulty
    item->strength.difficulty = compute_new_difficulty(item, response);

    // Update item state
    item->strength.current_stability = new_stability;
    item->interval_days = new_interval;
    item->last_review_time = current_time_days;
    item->next_review_time = current_time_days + new_interval;
    item->due_time = item->next_review_time;
    item->strength.retrievability = 1.0f;  // Just reviewed

    // Update state and stats
    if (old_state != new_state) {
        switch (old_state) {
            case SR_STATE_NEW:
                system->stats.new_items--;
                break;
            case SR_STATE_LEARNING:
                system->stats.learning_items--;
                break;
            case SR_STATE_REVIEW:
                system->stats.review_items--;
                break;
            case SR_STATE_RELEARNING:
                system->stats.relearning_items--;
                break;
            default:
                break;
        }
        switch (new_state) {
            case SR_STATE_NEW:
                system->stats.new_items++;
                break;
            case SR_STATE_LEARNING:
                system->stats.learning_items++;
                break;
            case SR_STATE_REVIEW:
                system->stats.review_items++;
                break;
            case SR_STATE_RELEARNING:
                system->stats.relearning_items++;
                break;
            default:
                break;
        }
    }
    item->state = new_state;

    // Create history record
    sr_review_record_t record = {
        .response = response,
        .time_taken_ms = response_time_ms,
        .retrievability = retrievability,
        .stability_before = stability_before,
        .stability_after = new_stability,
        .interval_days = new_interval,
        .timestamp_ms = timestamp_ms
    };
    item_add_history(item, &record);

    // Update response time average
    if (response_time_ms > 0 && isfinite(response_time_ms)) {
        float alpha = 0.1f;  // Exponential moving average factor
        item->avg_response_time_ms = alpha * response_time_ms +
            (1.0f - alpha) * item->avg_response_time_ms;
    }

    // Update system stats
    system->stats.total_reviews++;
    system->reviews_this_session++;
    system->reviews_today++;

    // Update heap position
    heap_update(system, item);

    // Sync with Z-ladder if enabled
    if (system->config.sync_with_z_ladder) {
        sr_sync_z_ladder(system, item_id);
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_system_undo_review(sr_system_t system, uint64_t item_id) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    if (item->history_len == 0) {
        sr_set_error("No review history to undo");
        return SR_ERROR_INVALID_TIME;
    }

    // Get last review record
    size_t last_idx = (item->history_start + item->history_len - 1) % item->history_capacity;
    sr_review_record_t* last_review = &item->response_history[last_idx];

    // Restore previous stability
    item->strength.current_stability = last_review->stability_before;

    // Decrement repetition count if was successful
    if (last_review->response != SR_RESPONSE_AGAIN && item->repetition_count > 0) {
        item->repetition_count--;
    }

    // If was a lapse, decrement lapses
    if (last_review->response == SR_RESPONSE_AGAIN && item->lapses > 0) {
        item->lapses--;
    }

    // Remove from history
    item->history_len--;

    // Recompute interval and due time
    if (item->history_len > 0) {
        size_t prev_idx = (item->history_start + item->history_len - 1) % item->history_capacity;
        item->last_review_time = sr_ms_to_days(item->response_history[prev_idx].timestamp_ms);
        item->interval_days = sr_interval_from_stability(
            item->strength.current_stability,
            system->config.target_retention
        );
    } else {
        // No history - reset to new state
        item->state = SR_STATE_NEW;
        item->last_review_time = 0;
        item->interval_days = 0;
    }

    item->due_time = item->last_review_time + item->interval_days;
    item->next_review_time = item->due_time;

    // Update heap
    heap_update(system, item);

    // Decrement review counts
    system->stats.total_reviews--;
    if (system->reviews_this_session > 0) {
        system->reviews_this_session--;
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_system_get_due(
    sr_system_t system,
    sr_spaced_item_t** items,
    size_t max_items,
    size_t* count
) {
    if (!system || !items || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    *count = 0;
    float current_time = sr_current_time_days();

    // Scan heap (items are partially ordered)
    for (size_t i = 0; i < system->heap_size && *count < max_items; i++) {
        sr_spaced_item_t* item = system->heap[i]->entry.item;
        if (item->due_time <= current_time && !item->is_suspended) {
            items[*count] = item;
            (*count)++;
        }
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT size_t sr_system_get_due_count(sr_system_t system) {
    if (!system) {
        return 0;
    }

    size_t count = 0;
    float current_time = sr_current_time_days();

    for (size_t i = 0; i < system->heap_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->heap_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->heap_size);
        }

        sr_spaced_item_t* item = system->heap[i]->entry.item;
        if (item->due_time <= current_time && !item->is_suspended) {
            count++;
        }
    }

    return count;
}

NIMCP_EXPORT sr_error_t sr_system_get_new(
    sr_system_t system,
    sr_spaced_item_t** items,
    size_t max_items,
    size_t* count
) {
    if (!system || !items || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    *count = 0;

    for (size_t i = 0; i < system->table_size && *count < max_items; i++) {
        sr_hash_entry_t* entry = system->items_table[i];
        while (entry && *count < max_items) {
            if (entry->item->state == SR_STATE_NEW && !entry->item->is_suspended) {
                items[*count] = entry->item;
                (*count)++;
            }
            entry = entry->next;
        }
    }

    return SR_SUCCESS;
}

//=============================================================================
// Scheduling Algorithm Functions
//=============================================================================

NIMCP_EXPORT sr_error_t sr_compute_interval(
    sr_system_t system,
    const sr_spaced_item_t* item,
    sr_review_response_t response,
    sr_schedule_result_t* result
) {
    if (!system || !item || !result) {
        return SR_ERROR_NULL_POINTER;
    }
    if (response < SR_RESPONSE_AGAIN || response > SR_RESPONSE_EASY) {
        return SR_ERROR_INVALID_RESPONSE;
    }

    float current_time = sr_current_time_days();
    float new_stability = 0.0f;
    float new_interval = 0.0f;
    sr_item_state_t new_state;

    if (response == SR_RESPONSE_AGAIN) {
        const sr_fsrs_params_t* p = &system->config.fsrs_params;
        new_stability = p->initial_stability *
            powf(item->strength.difficulty, -p->lapse_stability_factor) *
            powf((float)item->lapses + 2.0f, -p->lapse_decay_exponent);

        if (new_stability < system->config.min_interval_days) {
            new_stability = system->config.min_interval_days;
        }

        new_state = SR_STATE_RELEARNING;
        new_interval = system->config.relearning_steps_minutes[0] / 1440.0f;
    } else {
        // Create temporary item copy for calculation
        sr_spaced_item_t temp = *item;
        new_stability = compute_stability_increase(system, &temp, response);

        new_interval = sr_interval_from_stability(new_stability, system->config.target_retention);
        new_interval *= system->config.interval_modifier;

        if (response == SR_RESPONSE_EASY) {
            new_interval *= system->config.easy_bonus;
        } else if (response == SR_RESPONSE_HARD) {
            new_interval *= system->config.hard_penalty;
        }

        if (new_interval < system->config.min_interval_days) {
            new_interval = system->config.min_interval_days;
        }
        if (new_interval > system->config.max_interval_days) {
            new_interval = system->config.max_interval_days;
        }

        new_state = SR_STATE_REVIEW;
    }

    result->interval_days = new_interval;
    result->next_due_time = current_time + new_interval;
    result->new_stability = new_stability;
    result->predicted_retention_at_due = sr_predict_retention(item, new_interval);
    result->new_state = new_state;

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_preview_responses(
    sr_system_t system,
    const sr_spaced_item_t* item,
    sr_schedule_result_t* again_result,
    sr_schedule_result_t* hard_result,
    sr_schedule_result_t* good_result,
    sr_schedule_result_t* easy_result
) {
    sr_error_t err;

    if (again_result) {
        err = sr_compute_interval(system, item, SR_RESPONSE_AGAIN, again_result);
        if (err != SR_SUCCESS) return err;
    }
    if (hard_result) {
        err = sr_compute_interval(system, item, SR_RESPONSE_HARD, hard_result);
        if (err != SR_SUCCESS) return err;
    }
    if (good_result) {
        err = sr_compute_interval(system, item, SR_RESPONSE_GOOD, good_result);
        if (err != SR_SUCCESS) return err;
    }
    if (easy_result) {
        err = sr_compute_interval(system, item, SR_RESPONSE_EASY, easy_result);
        if (err != SR_SUCCESS) return err;
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT float sr_predict_retention(const sr_spaced_item_t* item, float days_from_now) {
    if (!item || days_from_now < 0) {
        return 0.0f;
    }
    if (item->strength.current_stability <= 0) {
        return 0.0f;
    }

    // R(t) = e^(-t/S)
    return expf(-days_from_now / item->strength.current_stability);
}

NIMCP_EXPORT float sr_predict_retention_at(const sr_spaced_item_t* item, uint64_t timestamp_ms) {
    if (!item) {
        return 0.0f;
    }

    float target_time = sr_ms_to_days(timestamp_ms);
    float time_elapsed = target_time - item->last_review_time;

    if (time_elapsed < 0) {
        return 1.0f;  // Before last review
    }

    return sr_predict_retention(item, time_elapsed);
}

NIMCP_EXPORT size_t sr_system_apply_forgetting(sr_system_t system) {
    if (!system) {
        return 0;
    }

    size_t count = 0;
    float current_time = sr_current_time_days();

    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            sr_spaced_item_t* item = entry->item;
            if (!item->is_suspended && item->last_review_time > 0) {
                float time_elapsed = current_time - item->last_review_time;
                if (time_elapsed > 0) {
                    item->strength.retrievability = sr_predict_retention(item, time_elapsed);
                    count++;
                }
            }
            entry = entry->next;
        }
    }

    return count;
}

NIMCP_EXPORT float sr_compute_priority(sr_system_t system, const sr_spaced_item_t* item) {
    if (!system || !item) {
        return 0.0f;
    }
    return compute_entry_priority(system, item);
}

NIMCP_EXPORT sr_error_t sr_system_reschedule(
    sr_system_t system,
    uint64_t item_id,
    float interval_days
) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    float current_time = sr_current_time_days();
    item->interval_days = interval_days;
    item->due_time = current_time + interval_days;
    item->next_review_time = item->due_time;

    // Update stability to match interval
    item->strength.current_stability = sr_stability_from_interval(
        interval_days,
        system->config.target_retention
    );

    heap_update(system, item);

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_system_reset_item(sr_system_t system, uint64_t item_id) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    // Update stats
    switch (item->state) {
        case SR_STATE_NEW:
            break;  // Already counted
        case SR_STATE_LEARNING:
            system->stats.learning_items--;
            system->stats.new_items++;
            break;
        case SR_STATE_REVIEW:
            system->stats.review_items--;
            system->stats.new_items++;
            break;
        case SR_STATE_RELEARNING:
            system->stats.relearning_items--;
            system->stats.new_items++;
            break;
        default:
            break;
    }

    if (item->is_leech) {
        system->stats.leech_items--;
    }

    // Reset item state
    item->state = SR_STATE_NEW;
    item->strength.current_stability = system->config.fsrs_params.initial_stability;
    item->strength.difficulty = SR_DEFAULT_DIFFICULTY;
    item->strength.retrievability = 1.0f;
    item->interval_days = 0;
    item->repetition_count = 0;
    item->lapses = 0;
    item->last_review_time = 0;
    item->due_time = sr_current_time_days();
    item->next_review_time = item->due_time;
    item->history_len = 0;
    item->history_start = 0;
    item->is_leech = false;
    item->avg_response_time_ms = 0;

    heap_update(system, item);

    return SR_SUCCESS;
}

//=============================================================================
// Schedule Optimization Functions
//=============================================================================

NIMCP_EXPORT sr_error_t sr_optimize_schedule(
    sr_system_t system,
    float available_minutes,
    uint64_t* optimized_order,
    size_t max_items,
    size_t* count
) {
    if (!system || !optimized_order || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    *count = 0;

    // Get all due items
    size_t due_count = sr_system_get_due_count(system);
    if (due_count == 0) {
        return SR_SUCCESS;
    }

    // Allocate temporary array for sorting
    typedef struct {
        sr_spaced_item_t* item;
        float value;  // retention impact per minute
    } item_value_t;

    item_value_t* items = (item_value_t*)nimcp_calloc(due_count, sizeof(item_value_t));
    if (!items) {
        return SR_ERROR_NO_MEMORY;
    }

    // Gather due items and compute value
    size_t idx = 0;
    float current_time = sr_current_time_days();

    for (size_t i = 0; i < system->heap_size && idx < due_count; i++) {
        sr_spaced_item_t* item = system->heap[i]->entry.item;
        if (item->due_time <= current_time && !item->is_suspended) {
            // Value = retention loss prevented / estimated time
            float current_ret = item->strength.retrievability;
            float time_estimate = (item->avg_response_time_ms > 0)
                ? item->avg_response_time_ms / 60000.0f  // Convert to minutes
                : 0.5f;  // Default 30 seconds

            // Retention loss if not reviewed now
            float loss = (1.0f - current_ret);

            items[idx].item = item;
            items[idx].value = loss / time_estimate;
            idx++;
        }
    }

    // Sort by value (descending)
    for (size_t i = 0; i < idx - 1; i++) {
        for (size_t j = i + 1; j < idx; j++) {
            if (items[j].value > items[i].value) {
                item_value_t temp = items[i];
                items[i] = items[j];
                items[j] = temp;
            }
        }
    }

    // Select items within time budget
    float time_used = 0;
    for (size_t i = 0; i < idx && *count < max_items; i++) {
        float item_time = (items[i].item->avg_response_time_ms > 0)
            ? items[i].item->avg_response_time_ms / 60000.0f
            : 0.5f;

        if (time_used + item_time <= available_minutes) {
            optimized_order[*count] = items[i].item->item_id;
            (*count)++;
            time_used += item_time;
        }
    }

    nimcp_free(items);
    items = NULL;
    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_balance_workload(
    sr_system_t system,
    size_t target_daily_reviews,
    size_t* recommended_new
) {
    if (!system || !recommended_new) {
        return SR_ERROR_NULL_POINTER;
    }

    // Estimate reviews coming from existing items
    size_t due_today = system->stats.due_today;

    // Calculate how many new items can fit
    if (due_today >= target_daily_reviews) {
        *recommended_new = 0;
    } else {
        size_t remaining = target_daily_reviews - due_today;

        // Each new item generates ~3-4 reviews in first week
        // Be conservative: new items take about 2x average review time
        size_t max_new = remaining / 2;

        // Cap at configured maximum
        if (max_new > system->config.max_new_per_day) {
            max_new = system->config.max_new_per_day;
        }

        // Cap at available new items
        if (max_new > system->stats.new_items) {
            max_new = system->stats.new_items;
        }

        *recommended_new = max_new;
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_estimate_workload(
    sr_system_t system,
    size_t days_ahead,
    sr_workload_forecast_t* forecast
) {
    if (!system || !forecast || days_ahead == 0) {
        return SR_ERROR_NULL_POINTER;
    }

    float current_time = sr_current_time_days();

    // Initialize forecast
    for (size_t d = 0; d < days_ahead; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && days_ahead > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(d + 1) / (float)days_ahead);
        }

        forecast[d].day_offset = (float)d;
        forecast[d].due_count = 0;
        forecast[d].cumulative_due = 0;
        forecast[d].estimated_minutes = 0;
    }

    // Count items due on each day
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            sr_spaced_item_t* item = entry->item;
            if (!item->is_suspended) {
                float days_until = item->due_time - current_time;
                if (days_until < 0) {
                    days_until = 0;  // Overdue items count for today
                }

                size_t day_idx = (size_t)days_until;
                if (day_idx < days_ahead) {
                    forecast[day_idx].due_count++;

                    float time_estimate = (item->avg_response_time_ms > 0)
                        ? item->avg_response_time_ms / 60000.0f
                        : 0.5f;
                    forecast[day_idx].estimated_minutes += time_estimate;
                }
            }
            entry = entry->next;
        }
    }

    // Compute cumulative
    size_t cumulative = 0;
    for (size_t d = 0; d < days_ahead; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && days_ahead > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(d + 1) / (float)days_ahead);
        }

        cumulative += forecast[d].due_count;
        forecast[d].cumulative_due = cumulative;
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_get_upcoming_due(
    sr_system_t system,
    float within_days,
    sr_spaced_item_t** items,
    size_t max_items,
    size_t* count
) {
    if (!system || !items || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    *count = 0;
    float current_time = sr_current_time_days();
    float cutoff = current_time + within_days;

    for (size_t i = 0; i < system->heap_size && *count < max_items; i++) {
        sr_spaced_item_t* item = system->heap[i]->entry.item;
        if (item->due_time <= cutoff && !item->is_suspended) {
            items[*count] = item;
            (*count)++;
        }
    }

    return SR_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT sr_error_t sr_system_get_stats(sr_system_t system, sr_stats_t* stats) {
    if (!system || !stats) {
        return SR_ERROR_NULL_POINTER;
    }

    // Copy cached stats
    *stats = system->stats;

    // Update dynamic stats
    stats->due_now = sr_system_get_due_count(system);

    // Calculate reviews per day average
    if (system->history_days > 0) {
        size_t total = 0;
        for (size_t i = 0; i < system->history_days; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->history_days > 256) {
                spaced_repetition_heartbeat("spaced_repet_loop",
                                 (float)(i + 1) / (float)system->history_days);
            }

            total += system->daily_review_counts[i];
        }
        stats->reviews_per_day_avg = (float)total / (float)system->history_days;
    }

    // Estimate time for due items
    float total_time = 0;
    float current_time = sr_current_time_days();
    for (size_t i = 0; i < system->heap_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->heap_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->heap_size);
        }

        sr_spaced_item_t* item = system->heap[i]->entry.item;
        if (item->due_time <= current_time && !item->is_suspended) {
            total_time += (item->avg_response_time_ms > 0)
                ? item->avg_response_time_ms / 60000.0f
                : 0.5f;
        }
    }
    stats->estimated_time_minutes = total_time;

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_get_retention_history(
    sr_system_t system,
    size_t days_back,
    float* retention_values,
    size_t* count
) {
    if (!system || !retention_values || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    size_t available = (days_back < system->history_days) ? days_back : system->history_days;
    *count = available;

    for (size_t i = 0; i < available; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && available > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)available);
        }

        retention_values[i] = system->daily_retention_rates[i];
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_get_review_history(
    sr_system_t system,
    size_t days_back,
    size_t* review_counts,
    size_t* count
) {
    if (!system || !review_counts || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    size_t available = (days_back < system->history_days) ? days_back : system->history_days;
    *count = available;

    for (size_t i = 0; i < available; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && available > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)available);
        }

        review_counts[i] = system->daily_review_counts[i];
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_get_difficulty_distribution(sr_system_t system, size_t bins[10]) {
    if (!system || !bins) {
        return SR_ERROR_NULL_POINTER;
    }

    memset(bins, 0, 10 * sizeof(size_t));

    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            float diff = entry->item->strength.difficulty;
            size_t bin = (size_t)(diff - 1.0f);
            if (bin >= 10) bin = 9;
            bins[bin]++;
            entry = entry->next;
        }
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_get_interval_distribution(
    sr_system_t system,
    const float* brackets,
    size_t num_brackets,
    size_t* counts
) {
    if (!system || !brackets || !counts || num_brackets == 0) {
        return SR_ERROR_NULL_POINTER;
    }

    memset(counts, 0, (num_brackets + 1) * sizeof(size_t));

    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            float interval = entry->item->interval_days;
            size_t bucket = num_brackets;  // Default to last bucket

            for (size_t b = 0; b < num_brackets; b++) {
                /* Phase 8: Loop progress heartbeat */
                if ((b & 0xFF) == 0 && num_brackets > 256) {
                    spaced_repetition_heartbeat("spaced_repet_loop",
                                     (float)(b + 1) / (float)num_brackets);
                }

                if (interval < brackets[b]) {
                    bucket = b;
                    break;
                }
            }
            counts[bucket]++;
            entry = entry->next;
        }
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT float sr_calculate_true_retention(sr_system_t system, size_t days_back) {
    if (!system) {
        return 0.0f;
    }

    size_t correct = 0;
    size_t total = 0;

    uint64_t cutoff_ms = (days_back > 0)
        ? sr_current_time_ms() - (days_back * 86400000ULL)
        : 0;

    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            sr_spaced_item_t* item = entry->item;
            for (size_t h = 0; h < item->history_len; h++) {
                /* Phase 8: Loop progress heartbeat */
                if ((h & 0xFF) == 0 && item->history_len > 256) {
                    spaced_repetition_heartbeat("spaced_repet_loop",
                                     (float)(h + 1) / (float)item->history_len);
                }

                size_t idx = (item->history_start + h) % item->history_capacity;
                sr_review_record_t* rec = &item->response_history[idx];

                if (days_back == 0 || rec->timestamp_ms >= cutoff_ms) {
                    total++;
                    if (rec->response != SR_RESPONSE_AGAIN) {
                        correct++;
                    }
                }
            }
            entry = entry->next;
        }
    }

    if (total == 0) {
        return 1.0f;  // No reviews = 100% by default
    }

    return (float)correct / (float)total;
}

//=============================================================================
// PR Integration Functions
//=============================================================================

NIMCP_EXPORT sr_error_t sr_sync_z_ladder(sr_system_t system, uint64_t item_id) {
    if (!system) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item || !item->memory) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    // Map stability to Z-tier
    // < 1 day: Z0 (working memory)
    // < 7 days: Z1 (short-term)
    // < 30 days: Z2 (long-term)
    // >= 30 days: Z3 (permanent)

    float stability = item->strength.current_stability;
    pr_memory_tier_t tier;

    if (stability < 1.0f) {
        tier = PR_MEMORY_TIER_Z0;
    } else if (stability < 7.0f) {
        tier = PR_MEMORY_TIER_Z1;
    } else if (stability < 30.0f) {
        tier = PR_MEMORY_TIER_Z2;
    } else {
        tier = PR_MEMORY_TIER_Z3;
    }

    pr_memory_node_set_tier(item->memory, tier);

    return SR_SUCCESS;
}

NIMCP_EXPORT size_t sr_sync_all_z_ladder(sr_system_t system) {
    if (!system) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            if (entry->item->memory) {
                sr_sync_z_ladder(system, entry->item->item_id);
                count++;
            }
            entry = entry->next;
        }
    }

    return count;
}

NIMCP_EXPORT sr_error_t sr_generate_cues(
    sr_system_t system,
    uint64_t item_id,
    uint64_t* cue_ids,
    size_t max_cues,
    size_t* count
) {
    if (!system || !cue_ids || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    *count = 0;

    if (!system->entanglement) {
        return SR_SUCCESS;  // No entanglement graph configured
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item || !item->memory) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    uint64_t memory_id = pr_memory_node_get_id(item->memory);

    // Get strongest connected memories from entanglement graph
    entangle_neighbor_t* neighbors = (entangle_neighbor_t*)nimcp_malloc(
        max_cues * sizeof(entangle_neighbor_t));
    if (!neighbors) {
        return SR_ERROR_NO_MEMORY;
    }

    size_t neighbor_count = 0;
    if (entangle_get_strongest(system->entanglement, memory_id, max_cues,
                                neighbors, &neighbor_count)) {
        for (size_t i = 0; i < neighbor_count && *count < max_cues; i++) {
            cue_ids[*count] = neighbors[i].neighbor_id;
            (*count)++;
        }
    }

    nimcp_free(neighbors);
    neighbors = NULL;
    return SR_SUCCESS;
}

NIMCP_EXPORT sr_error_t sr_find_similar_items(
    sr_system_t system,
    uint64_t item_id,
    uint64_t* similar_ids,
    size_t max_similar,
    size_t* count
) {
    if (!system || !similar_ids || !count) {
        return SR_ERROR_NULL_POINTER;
    }

    *count = 0;

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    // Find items with similar prime signatures
    typedef struct {
        uint64_t id;
        float similarity;
    } similar_entry_t;

    similar_entry_t* candidates = (similar_entry_t*)nimcp_malloc(
        system->num_items * sizeof(similar_entry_t));
    if (!candidates) {
        return SR_ERROR_NO_MEMORY;
    }

    size_t candidate_count = 0;

    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            if (entry->item->item_id != item_id) {
                float sim = prime_sig_jaccard(&item->content_signature,
                                               &entry->item->content_signature);
                if (sim > 0.3f) {  // Threshold for "similar"
                    candidates[candidate_count].id = entry->item->item_id;
                    candidates[candidate_count].similarity = sim;
                    candidate_count++;
                }
            }
            entry = entry->next;
        }
    }

    // Sort by similarity (descending)
    for (size_t i = 0; i < candidate_count - 1 && i < max_similar; i++) {
        for (size_t j = i + 1; j < candidate_count; j++) {
            if (candidates[j].similarity > candidates[i].similarity) {
                similar_entry_t temp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = temp;
            }
        }
    }

    // Return top matches
    for (size_t i = 0; i < candidate_count && *count < max_similar; i++) {
        similar_ids[*count] = candidates[i].id;
        (*count)++;
    }

    nimcp_free(candidates);
    candidates = NULL;
    return SR_SUCCESS;
}

//=============================================================================
// Serialization Functions
//=============================================================================

NIMCP_EXPORT sr_error_t sr_system_serialize(
    sr_system_t system,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
) {
    if (!system || !written_size) {
        return SR_ERROR_NULL_POINTER;
    }

    // Calculate required size
    size_t required = sizeof(uint32_t) * 2;  // Magic + version
    required += sizeof(sr_config_t);
    required += sizeof(uint64_t);  // num_items
    required += sizeof(uint64_t);  // next_item_id

    // Each item
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            required += sizeof(uint64_t);  // item_id
            required += sizeof(sr_memory_strength_t);
            required += sizeof(sr_item_state_t);
            required += sizeof(float) * 5;  // intervals, times
            required += sizeof(size_t) * 2;  // repetitions, lapses
            required += sizeof(bool) * 2;  // flags
            required += sizeof(size_t);  // history_len
            required += entry->item->history_len * sizeof(sr_review_record_t);
            entry = entry->next;
        }
    }

    *written_size = required;

    if (!buffer) {
        return SR_SUCCESS;  // Just querying size
    }
    if (buffer_size < required) {
        return SR_ERROR_SERIALIZATION;
    }

    uint8_t* ptr = (uint8_t*)buffer;

    // Magic and version
    *(uint32_t*)ptr = 0x53525300;  // "SRS\0"
    ptr += sizeof(uint32_t);
    *(uint32_t*)ptr = 1;  // Version 1
    ptr += sizeof(uint32_t);

    // Config
    memcpy(ptr, &system->config, sizeof(sr_config_t));
    ptr += sizeof(sr_config_t);

    // Counts
    *(uint64_t*)ptr = system->num_items;
    ptr += sizeof(uint64_t);
    *(uint64_t*)ptr = system->next_item_id;
    ptr += sizeof(uint64_t);

    // Items
    for (size_t i = 0; i < system->table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->table_size > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)system->table_size);
        }

        sr_hash_entry_t* entry = system->items_table[i];
        while (entry) {
            sr_spaced_item_t* item = entry->item;

            *(uint64_t*)ptr = item->item_id;
            ptr += sizeof(uint64_t);

            memcpy(ptr, &item->strength, sizeof(sr_memory_strength_t));
            ptr += sizeof(sr_memory_strength_t);

            *(sr_item_state_t*)ptr = item->state;
            ptr += sizeof(sr_item_state_t);

            *(float*)ptr = item->interval_days;
            ptr += sizeof(float);
            *(float*)ptr = item->last_review_time;
            ptr += sizeof(float);
            *(float*)ptr = item->next_review_time;
            ptr += sizeof(float);
            *(float*)ptr = item->due_time;
            ptr += sizeof(float);
            *(float*)ptr = item->avg_response_time_ms;
            ptr += sizeof(float);

            *(size_t*)ptr = item->repetition_count;
            ptr += sizeof(size_t);
            *(size_t*)ptr = item->lapses;
            ptr += sizeof(size_t);

            *(bool*)ptr = item->is_leech;
            ptr += sizeof(bool);
            *(bool*)ptr = item->is_suspended;
            ptr += sizeof(bool);

            *(size_t*)ptr = item->history_len;
            ptr += sizeof(size_t);

            for (size_t h = 0; h < item->history_len; h++) {
                /* Phase 8: Loop progress heartbeat */
                if ((h & 0xFF) == 0 && item->history_len > 256) {
                    spaced_repetition_heartbeat("spaced_repet_loop",
                                     (float)(h + 1) / (float)item->history_len);
                }

                size_t idx = (item->history_start + h) % item->history_capacity;
                memcpy(ptr, &item->response_history[idx], sizeof(sr_review_record_t));
                ptr += sizeof(sr_review_record_t);
            }

            entry = entry->next;
        }
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT sr_system_t sr_system_deserialize(
    const void* buffer,
    size_t buffer_size,
    size_t* bytes_read
) {
    if (!buffer || buffer_size < sizeof(uint32_t) * 2) {
        sr_set_error("Invalid buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sr_system_deserialize: buffer is NULL");
        return NULL;
    }

    const uint8_t* ptr = (const uint8_t*)buffer;

    // Check magic
    uint32_t magic = *(const uint32_t*)ptr;
    ptr += sizeof(uint32_t);
    if (magic != 0x53525300) {
        sr_set_error("Invalid magic number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_system_deserialize: validation failed");
        return NULL;
    }

    uint32_t version = *(const uint32_t*)ptr;
    ptr += sizeof(uint32_t);
    if (version != 1) {
        sr_set_error("Unsupported version");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_system_deserialize: validation failed");
        return NULL;
    }

    // Read config
    sr_config_t config;
    memcpy(&config, ptr, sizeof(sr_config_t));
    ptr += sizeof(sr_config_t);

    // Create system
    sr_system_t system = sr_system_create(&config);
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    // Read counts
    uint64_t num_items = *(const uint64_t*)ptr;
    ptr += sizeof(uint64_t);
    system->next_item_id = *(const uint64_t*)ptr;
    ptr += sizeof(uint64_t);

    // Note: Items need associated memory nodes to be useful
    // This deserializes the scheduling state only

    if (bytes_read) {
        *bytes_read = (size_t)(ptr - (const uint8_t*)buffer);
    }

    (void)num_items;  // Items would need memory association

    return system;
}

NIMCP_EXPORT sr_error_t sr_export_item(
    sr_system_t system,
    uint64_t item_id,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
) {
    if (!system || !written_size) {
        return SR_ERROR_NULL_POINTER;
    }

    sr_spaced_item_t* item = hash_lookup(system, item_id);
    if (!item) {
        return SR_ERROR_ITEM_NOT_FOUND;
    }

    size_t required = sizeof(uint64_t);  // item_id
    required += sizeof(sr_memory_strength_t);
    required += sizeof(sr_item_state_t);
    required += sizeof(float) * 5;
    required += sizeof(size_t) * 2;
    required += sizeof(bool) * 2;
    required += sizeof(size_t);
    required += item->history_len * sizeof(sr_review_record_t);

    *written_size = required;

    if (!buffer) {
        return SR_SUCCESS;
    }
    if (buffer_size < required) {
        return SR_ERROR_SERIALIZATION;
    }

    // Similar to serialize but for single item
    uint8_t* ptr = (uint8_t*)buffer;

    *(uint64_t*)ptr = item->item_id;
    ptr += sizeof(uint64_t);

    memcpy(ptr, &item->strength, sizeof(sr_memory_strength_t));
    ptr += sizeof(sr_memory_strength_t);

    *(sr_item_state_t*)ptr = item->state;
    ptr += sizeof(sr_item_state_t);

    *(float*)ptr = item->interval_days;
    ptr += sizeof(float);
    *(float*)ptr = item->last_review_time;
    ptr += sizeof(float);
    *(float*)ptr = item->next_review_time;
    ptr += sizeof(float);
    *(float*)ptr = item->due_time;
    ptr += sizeof(float);
    *(float*)ptr = item->avg_response_time_ms;
    ptr += sizeof(float);

    *(size_t*)ptr = item->repetition_count;
    ptr += sizeof(size_t);
    *(size_t*)ptr = item->lapses;
    ptr += sizeof(size_t);

    *(bool*)ptr = item->is_leech;
    ptr += sizeof(bool);
    *(bool*)ptr = item->is_suspended;
    ptr += sizeof(bool);

    *(size_t*)ptr = item->history_len;
    ptr += sizeof(size_t);

    for (size_t h = 0; h < item->history_len; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && item->history_len > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(h + 1) / (float)item->history_len);
        }

        size_t idx = (item->history_start + h) % item->history_capacity;
        memcpy(ptr, &item->response_history[idx], sizeof(sr_review_record_t));
        ptr += sizeof(sr_review_record_t);
    }

    return SR_SUCCESS;
}

NIMCP_EXPORT uint64_t sr_import_item(
    sr_system_t system,
    const void* buffer,
    size_t buffer_size,
    pr_memory_node_t* memory
) {
    if (!system || !buffer || !memory) {
        return 0;
    }

    // Add item with memory
    uint64_t new_id = sr_system_add_item(system, memory);
    if (new_id == 0) {
        return 0;
    }

    sr_spaced_item_t* item = hash_lookup(system, new_id);
    if (!item) {
        return 0;
    }

    // Restore state from buffer
    const uint8_t* ptr = (const uint8_t*)buffer;

    ptr += sizeof(uint64_t);  // Skip original item_id

    memcpy(&item->strength, ptr, sizeof(sr_memory_strength_t));
    ptr += sizeof(sr_memory_strength_t);

    item->state = *(const sr_item_state_t*)ptr;
    ptr += sizeof(sr_item_state_t);

    item->interval_days = *(const float*)ptr;
    ptr += sizeof(float);
    item->last_review_time = *(const float*)ptr;
    ptr += sizeof(float);
    item->next_review_time = *(const float*)ptr;
    ptr += sizeof(float);
    item->due_time = *(const float*)ptr;
    ptr += sizeof(float);
    item->avg_response_time_ms = *(const float*)ptr;
    ptr += sizeof(float);

    item->repetition_count = *(const size_t*)ptr;
    ptr += sizeof(size_t);
    item->lapses = *(const size_t*)ptr;
    ptr += sizeof(size_t);

    item->is_leech = *(const bool*)ptr;
    ptr += sizeof(bool);
    item->is_suspended = *(const bool*)ptr;
    ptr += sizeof(bool);

    size_t history_len = *(const size_t*)ptr;
    ptr += sizeof(size_t);

    // Import history
    for (size_t h = 0; h < history_len && h < item->history_capacity; h++) {
        sr_review_record_t record;
        memcpy(&record, ptr, sizeof(sr_review_record_t));
        ptr += sizeof(sr_review_record_t);
        item_add_history(item, &record);
    }

    // Update heap position
    heap_update(system, item);

    (void)buffer_size;  // Would add bounds checking in production

    return new_id;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* sr_error_string(sr_error_t error) {
    switch (error) {
        case SR_SUCCESS:              return "Success";
        case SR_ERROR_NULL_POINTER:   return "Null pointer";
        case SR_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case SR_ERROR_NO_MEMORY:      return "Memory allocation failed";
        case SR_ERROR_ITEM_NOT_FOUND: return "Item not found";
        case SR_ERROR_ITEM_EXISTS:    return "Item already exists";
        case SR_ERROR_INVALID_RESPONSE: return "Invalid response";
        case SR_ERROR_QUEUE_EMPTY:    return "Queue is empty";
        case SR_ERROR_QUEUE_FULL:     return "Queue is full";
        case SR_ERROR_SUSPENDED:      return "Item is suspended";
        case SR_ERROR_INVALID_TIME:   return "Invalid timestamp";
        case SR_ERROR_SERIALIZATION:  return "Serialization failed";
        case SR_ERROR_DESERIALIZATION: return "Deserialization failed";
        default:                      return "Unknown error";
    }
}

NIMCP_EXPORT const char* sr_get_last_error(void) {
    if (sr_error_buffer[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sr_get_last_error: validation failed");
        return NULL;
    }
    return sr_error_buffer;
}

NIMCP_EXPORT const char* sr_response_string(sr_review_response_t response) {
    switch (response) {
        case SR_RESPONSE_AGAIN: return "Again";
        case SR_RESPONSE_HARD:  return "Hard";
        case SR_RESPONSE_GOOD:  return "Good";
        case SR_RESPONSE_EASY:  return "Easy";
        default:                return "Unknown";
    }
}

NIMCP_EXPORT const char* sr_state_string(sr_item_state_t state) {
    switch (state) {
        case SR_STATE_NEW:        return "New";
        case SR_STATE_LEARNING:   return "Learning";
        case SR_STATE_REVIEW:     return "Review";
        case SR_STATE_RELEARNING: return "Relearning";
        case SR_STATE_SUSPENDED:  return "Suspended";
        case SR_STATE_BURIED:     return "Buried";
        default:                  return "Unknown";
    }
}

NIMCP_EXPORT float sr_current_time_days(void) {
    return sr_ms_to_days(sr_current_time_ms());
}

NIMCP_EXPORT uint64_t sr_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

NIMCP_EXPORT uint64_t sr_days_to_ms(float days) {
    return (uint64_t)(days * 86400000.0f);
}

NIMCP_EXPORT float sr_ms_to_days(uint64_t ms) {
    return (float)ms / 86400000.0f;
}

NIMCP_EXPORT size_t sr_format_interval(float interval_days, char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    if (interval_days < 1.0f / 1440.0f) {  // < 1 minute
        return snprintf(buffer, size, "<1m");
    } else if (interval_days < 1.0f / 24.0f) {  // < 1 hour
        int minutes = (int)(interval_days * 1440.0f);
        return snprintf(buffer, size, "%dm", minutes);
    } else if (interval_days < 1.0f) {  // < 1 day
        int hours = (int)(interval_days * 24.0f);
        return snprintf(buffer, size, "%dh", hours);
    } else if (interval_days < 7.0f) {
        int days = (int)interval_days;
        return snprintf(buffer, size, "%dd", days);
    } else if (interval_days < 30.0f) {
        int weeks = (int)(interval_days / 7.0f);
        return snprintf(buffer, size, "%dw", weeks);
    } else if (interval_days < 365.0f) {
        int months = (int)(interval_days / 30.0f);
        return snprintf(buffer, size, "%dmo", months);
    } else {
        float years = interval_days / 365.0f;
        return snprintf(buffer, size, "%.1fy", years);
    }
}

NIMCP_EXPORT void sr_item_print(const sr_spaced_item_t* item) {
    if (!item) {
        printf("NULL item\n");
        return;
    }

    char interval_str[16];
    sr_format_interval(item->interval_days, interval_str, sizeof(interval_str));

    printf("Item %lu [%s]: interval=%s, stability=%.2fd, difficulty=%.1f, "
           "reps=%zu, lapses=%zu, retrievability=%.1f%%\n",
           (unsigned long)item->item_id,
           sr_state_string(item->state),
           interval_str,
           item->strength.current_stability,
           item->strength.difficulty,
           item->repetition_count,
           item->lapses,
           item->strength.retrievability * 100.0f);
}

NIMCP_EXPORT void sr_system_print_summary(sr_system_t system) {
    if (!system) {
        printf("NULL system\n");
        return;
    }

    sr_stats_t stats;
    sr_system_get_stats(system, &stats);

    printf("=== Spaced Repetition System Summary ===\n");
    printf("Items: %zu total (%zu new, %zu learning, %zu review, %zu relearning)\n",
           stats.total_items, stats.new_items, stats.learning_items,
           stats.review_items, stats.relearning_items);
    printf("Reviews: %zu total, %.1f%% retention\n",
           stats.total_reviews, sr_calculate_true_retention(system, 0) * 100.0f);
    printf("Due: %zu now, %.1f min estimated\n",
           stats.due_now, stats.estimated_time_minutes);
    printf("Response distribution: Again=%zu, Hard=%zu, Good=%zu, Easy=%zu\n",
           stats.again_count, stats.hard_count, stats.good_count, stats.easy_count);
    if (stats.leech_items > 0) {
        printf("Leeches: %zu items\n", stats.leech_items);
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static uint64_t hash_id(uint64_t id, size_t table_size) {
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (int i = 0; i < 8; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 8 > 256) {
            spaced_repetition_heartbeat("spaced_repet_loop",
                             (float)(i + 1) / (float)8);
        }

        hash ^= (id >> (i * 8)) & 0xFF;
        hash *= 1099511628211ULL;
    }
    return hash % table_size;
}

static sr_spaced_item_t* hash_lookup(sr_system_t system, uint64_t item_id) {
    uint64_t idx = hash_id(item_id, system->table_size);
    sr_hash_entry_t* entry = system->items_table[idx];

    while (entry) {
        if (entry->item_id == item_id) {
            return entry->item;
        }
        entry = entry->next;
    }

    return NULL;
}

static void hash_insert(sr_system_t system, sr_spaced_item_t* item) {
    uint64_t idx = hash_id(item->item_id, system->table_size);

    sr_hash_entry_t* entry = (sr_hash_entry_t*)nimcp_malloc(sizeof(sr_hash_entry_t));
    if (!entry) return;
    entry->item_id = item->item_id;
    entry->item = item;
    entry->next = system->items_table[idx];
    system->items_table[idx] = entry;
    system->num_items++;
}

static void hash_remove(sr_system_t system, uint64_t item_id) {
    uint64_t idx = hash_id(item_id, system->table_size);
    sr_hash_entry_t** entry_ptr = &system->items_table[idx];

    while (*entry_ptr) {
        if ((*entry_ptr)->item_id == item_id) {
            sr_hash_entry_t* to_remove = *entry_ptr;
            *entry_ptr = to_remove->next;
            nimcp_free(to_remove);
            to_remove = NULL;
            system->num_items--;
            return;
        }
        entry_ptr = &(*entry_ptr)->next;
    }
}

static sr_spaced_item_t* item_create(sr_system_t system, pr_memory_node_t* memory) {
    sr_spaced_item_t* item = (sr_spaced_item_t*)nimcp_calloc(1, sizeof(sr_spaced_item_t));
    if (!item) {
        sr_set_error("Failed to allocate item");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "item_create: item is NULL");
        return NULL;
    }

    item->item_id = system->next_item_id++;
    item->memory = memory;

    // Copy content signature if available
    const prime_signature_t* sig = pr_memory_node_get_signature(memory);
    if (sig) {
        item->content_signature = *sig;
    }

    // Initialize strength
    item->strength.initial_stability = system->config.fsrs_params.initial_stability;
    item->strength.current_stability = system->config.fsrs_params.initial_stability;
    item->strength.stability_growth = 1.0f;
    item->strength.difficulty = SR_DEFAULT_DIFFICULTY;
    item->strength.retrievability = 1.0f;

    // Initialize state
    item->state = SR_STATE_NEW;
    item->interval_days = 0;
    item->repetition_count = 0;
    item->lapses = 0;

    // Initialize timing
    float now = sr_current_time_days();
    item->created_timestamp_ms = sr_current_time_ms();
    item->last_review_time = 0;
    item->next_review_time = now;
    item->due_time = now;

    // Allocate history buffer
    item->history_capacity = system->config.history_capacity;
    if (item->history_capacity > 0) {
        item->response_history = (sr_review_record_t*)nimcp_calloc(
            item->history_capacity, sizeof(sr_review_record_t));
        if (!item->response_history) {
            item->history_capacity = 0;
        }
    }

    return item;
}

static void item_destroy(sr_spaced_item_t* item) {
    if (!item) {
        return;
    }
    nimcp_free(item->response_history);
    nimcp_free(item);
    item = NULL;
}

static void item_add_history(sr_spaced_item_t* item, const sr_review_record_t* record) {
    if (!item->response_history || item->history_capacity == 0) {
        return;
    }

    size_t write_idx = 0;
    if (item->history_len < item->history_capacity) {
        write_idx = item->history_len;
        item->history_len++;
    } else {
        // Circular buffer: overwrite oldest
        write_idx = item->history_start;
        item->history_start = (item->history_start + 1) % item->history_capacity;
    }

    item->response_history[write_idx] = *record;
}

static float compute_stability_increase(
    sr_system_t system,
    const sr_spaced_item_t* item,
    sr_review_response_t response
) {
    const sr_fsrs_params_t* p = &system->config.fsrs_params;

    float S = item->strength.current_stability;
    float D = item->strength.difficulty;
    float R = item->strength.retrievability;

    // FSRS formula: S(n+1) = S(n) * (1 + a * D^(-b) * S(n)^(-c) * e^(d*(R-1)))
    float growth = p->param_a *
                   powf(D, -p->param_b) *
                   powf(S, -p->param_c) *
                   expf(p->param_d * (R - 1.0f));

    float new_stability = S * (1.0f + growth);

    // Apply response modifier
    switch (response) {
        case SR_RESPONSE_EASY:
            new_stability *= 1.2f;
            break;
        case SR_RESPONSE_HARD:
            new_stability *= 0.9f;
            break;
        default:
            break;
    }

    // Ensure minimum stability
    if (new_stability < system->config.min_interval_days) {
        new_stability = system->config.min_interval_days;
    }

    return new_stability;
}

static float compute_new_difficulty(const sr_spaced_item_t* item, sr_review_response_t response) {
    float D = item->strength.difficulty;

    // Adjust difficulty based on response
    switch (response) {
        case SR_RESPONSE_AGAIN:
            D += 0.2f;
            break;
        case SR_RESPONSE_HARD:
            D += 0.1f;
            break;
        case SR_RESPONSE_GOOD:
            // No change
            break;
        case SR_RESPONSE_EASY:
            D -= 0.15f;
            break;
    }

    // Clamp to valid range
    if (D < SR_MIN_DIFFICULTY) D = SR_MIN_DIFFICULTY;
    if (D > SR_MAX_DIFFICULTY) D = SR_MAX_DIFFICULTY;

    return D;
}

static float add_interval_fuzz(float interval) {
    // Add small random variation to prevent clustering
    // Fuzz is +/- 5% of interval, min 1 minute
    float fuzz_range = interval * SR_INTERVAL_FUZZ_FACTOR;
    if (fuzz_range < 1.0f / 1440.0f) {
        fuzz_range = 1.0f / 1440.0f;
    }

    // Simple deterministic "randomness" based on interval
    float pseudo_random = sinf(interval * 12345.0f);  // -1 to 1
    return interval + (pseudo_random * fuzz_range);
}

static float compute_entry_priority(sr_system_t system, const sr_spaced_item_t* item) {
    float current_time = sr_current_time_days();
    float overdue = current_time - item->due_time;

    // Base priority on overdue amount
    float priority = 0.0f;

    if (overdue > 0) {
        // Overdue items get high priority
        priority = 1.0f + (overdue * SR_OVERDUE_PRIORITY_BOOST);
    } else {
        // Not yet due: negative priority (will sort after due items)
        priority = overdue;  // Negative value
    }

    // Boost priority for items with low retrievability
    priority += (1.0f - item->strength.retrievability) * 0.5f;

    // Learning items get slight boost
    if (item->state == SR_STATE_LEARNING || item->state == SR_STATE_RELEARNING) {
        priority += 0.2f;
    }

    (void)system;  // Reserved for future config-based adjustments

    return priority;
}

//=============================================================================
// Heap Operations (Min-Heap by due_time)
//=============================================================================

static void heap_sift_up(sr_system_t system, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (system->heap[index]->entry.item->due_time <
            system->heap[parent]->entry.item->due_time) {
            // Swap
            sr_heap_node_t* temp = system->heap[index];
            system->heap[index] = system->heap[parent];
            system->heap[parent] = temp;

            system->heap[index]->heap_index = index;
            system->heap[parent]->heap_index = parent;

            index = parent;
        } else {
            break;
        }
    }
}

static void heap_sift_down(sr_system_t system, size_t index) {
    while (true) {
        size_t smallest = index;
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;

        if (left < system->heap_size &&
            system->heap[left]->entry.item->due_time <
            system->heap[smallest]->entry.item->due_time) {
            smallest = left;
        }
        if (right < system->heap_size &&
            system->heap[right]->entry.item->due_time <
            system->heap[smallest]->entry.item->due_time) {
            smallest = right;
        }

        if (smallest != index) {
            sr_heap_node_t* temp = system->heap[index];
            system->heap[index] = system->heap[smallest];
            system->heap[smallest] = temp;

            system->heap[index]->heap_index = index;
            system->heap[smallest]->heap_index = smallest;

            index = smallest;
        } else {
            break;
        }
    }
}

static void heap_insert(sr_system_t system, sr_spaced_item_t* item) {
    // Grow heap if needed
    if (system->heap_size >= system->heap_capacity) {
        size_t new_capacity = system->heap_capacity * 2;
        sr_heap_node_t** new_heap = (sr_heap_node_t**)nimcp_realloc(
            system->heap, new_capacity * sizeof(sr_heap_node_t*));
        if (!new_heap) {
            return;
        }
        system->heap = new_heap;
        system->heap_capacity = new_capacity;
    }

    // Create heap node
    sr_heap_node_t* node = (sr_heap_node_t*)nimcp_malloc(sizeof(sr_heap_node_t));
    if (!node) {
        return;
    }

    node->entry.item = item;
    node->entry.priority = compute_entry_priority(system, item);
    node->entry.overdue_days = sr_current_time_days() - item->due_time;
    node->entry.predicted_retention = item->strength.retrievability;
    node->entry.queue_timestamp_ms = sr_current_time_ms();
    node->heap_index = system->heap_size;

    // Store reference in item for O(1) lookup
    item->flags = (uint32_t)node->heap_index;  // Reusing flags for heap index

    system->heap[system->heap_size] = node;
    system->heap_size++;

    heap_sift_up(system, system->heap_size - 1);
}

static sr_spaced_item_t* heap_extract_min(sr_system_t system) {
    if (system->heap_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heap_extract_min: system->heap_size is zero");
        return NULL;
    }

    sr_heap_node_t* min_node = system->heap[0];
    sr_spaced_item_t* item = min_node->entry.item;

    // Move last to root
    system->heap[0] = system->heap[system->heap_size - 1];
    system->heap[0]->heap_index = 0;
    system->heap_size--;

    nimcp_free(min_node);
    min_node = NULL;

    if (system->heap_size > 0) {
        heap_sift_down(system, 0);
    }

    return item;
}

static void heap_update(sr_system_t system, sr_spaced_item_t* item) {
    // Find node in heap (using stored index)
    size_t index = item->flags;  // We stored heap index in flags
    if (index >= system->heap_size) {
        // Not in heap, re-insert
        heap_insert(system, item);
        return;
    }

    sr_heap_node_t* node = system->heap[index];
    if (node->entry.item != item) {
        // Index mismatch, linear search
        for (size_t i = 0; i < system->heap_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->heap_size > 256) {
                spaced_repetition_heartbeat("spaced_repet_loop",
                                 (float)(i + 1) / (float)system->heap_size);
            }

            if (system->heap[i]->entry.item == item) {
                node = system->heap[i];
                index = i;
                break;
            }
        }
    }

    // Update priority
    node->entry.priority = compute_entry_priority(system, item);
    node->entry.overdue_days = sr_current_time_days() - item->due_time;
    node->entry.predicted_retention = item->strength.retrievability;

    // Rebalance
    heap_sift_up(system, index);
    heap_sift_down(system, index);

    item->flags = (uint32_t)node->heap_index;
}

static void heap_remove(sr_system_t system, sr_spaced_item_t* item) {
    // Find node
    size_t index = item->flags;
    if (index >= system->heap_size) {
        return;
    }

    sr_heap_node_t* node = system->heap[index];
    if (node->entry.item != item) {
        // Linear search
        for (size_t i = 0; i < system->heap_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->heap_size > 256) {
                spaced_repetition_heartbeat("spaced_repet_loop",
                                 (float)(i + 1) / (float)system->heap_size);
            }

            if (system->heap[i]->entry.item == item) {
                node = system->heap[i];
                index = i;
                break;
            }
        }
        if (node->entry.item != item) {
            return;  // Not found
        }
    }

    // Move last element to this position
    sr_heap_node_t* last = system->heap[system->heap_size - 1];
    system->heap[index] = last;
    last->heap_index = index;
    system->heap_size--;

    nimcp_free(node);
    node = NULL;

    if (index < system->heap_size) {
        heap_sift_up(system, index);
        heap_sift_down(system, index);
    }
}

static void update_daily_stats(sr_system_t system) {
    uint64_t now_ms = sr_current_time_ms();
    uint64_t today_start = (now_ms / 86400000ULL) * 86400000ULL;

    if (today_start != system->day_start_ms) {
        // New day - shift history
        if (system->history_days < system->history_capacity) {
            system->history_days++;
        }

        // Shift arrays
        if (system->history_days > 1) {
            memmove(&system->daily_review_counts[1],
                    &system->daily_review_counts[0],
                    (system->history_days - 1) * sizeof(size_t));
            memmove(&system->daily_retention_rates[1],
                    &system->daily_retention_rates[0],
                    (system->history_days - 1) * sizeof(float));
        }

        // Record yesterday's stats
        system->daily_review_counts[0] = system->reviews_today;
        system->daily_retention_rates[0] = sr_calculate_true_retention(system, 1);

        // Reset for new day
        system->day_start_ms = today_start;
        system->reviews_today = 0;
        system->new_today = 0;
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void spaced_repetition_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_spaced_repetition_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int spaced_repetition_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "spaced_repetition_training_begin: NULL argument");
        return -1;
    }
    spaced_repetition_heartbeat_instance(NULL, "spaced_repetition_training_begin", 0.0f);
    (void)(struct sr_heap_node*)instance; /* Module state available for reset */
    return 0;
}

int spaced_repetition_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "spaced_repetition_training_end: NULL argument");
        return -1;
    }
    spaced_repetition_heartbeat_instance(NULL, "spaced_repetition_training_end", 1.0f);
    (void)(struct sr_heap_node*)instance; /* Module state available for finalization */
    return 0;
}

int spaced_repetition_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "spaced_repetition_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    spaced_repetition_heartbeat_instance(NULL, "spaced_repetition_training_step", progress);
    (void)(struct sr_heap_node*)instance; /* Module state available for step adaptation */
    return 0;
}
