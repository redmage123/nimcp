//=============================================================================
// nimcp_prospective_scheduler.c - Prime Resonant Prospective Memory Scheduler
//=============================================================================
/**
 * @file nimcp_prospective_scheduler.c
 * @brief Implementation of prospective memory scheduler
 *
 * WHAT: Full implementation of prospective memory scheduling operations
 * WHY:  Manage future intentions with priority queuing and reminder system
 * HOW:  Max-heap for priority management, time-ordered list for triggers,
 *       graduated reminder system with callbacks
 *
 * IMPLEMENTATION NOTES:
 * - Priority heap is a max-heap ordered by computed priority
 * - Time-ordered list maintained sorted by deadline for efficient time queries
 * - Conflict groups are arrays of intention IDs with resolution strategy
 * - All public functions validate inputs and return appropriate error codes
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_prospective_scheduler.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(prospective_scheduler, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** Minimum heap growth factor */
#define HEAP_GROWTH_FACTOR 2

/** Initial conflict group capacity */
#define INITIAL_CONFLICT_CAPACITY 8

//=============================================================================
// Internal Helper Functions - Time
//=============================================================================

/**
 * @brief Get current time in milliseconds (monotonic if available)
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;

#ifdef CLOCK_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
#endif

    // Fallback to real time
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }

    // Last resort fallback
    return (uint64_t)time(NULL) * 1000ULL;
}

//=============================================================================
// Internal Helper Functions - Math
//=============================================================================

/**
 * @brief Safe string copy with null termination
 */
static void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

//=============================================================================
// Internal Helper Functions - Heap Operations
//=============================================================================

/**
 * @brief Swap two heap nodes
 */
static void heap_swap(priority_heap_node_t* heap, size_t i, size_t j) {
    priority_heap_node_t temp = heap[i];
    heap[i] = heap[j];
    heap[j] = temp;

    // Update heap indices in scheduled_intention_t
    if (heap[i].item) {
        heap[i].item->heap_index = i;
    }
    if (heap[j].item) {
        heap[j].item->heap_index = j;
    }
}

/**
 * @brief Sift up in max-heap
 */
static void heap_sift_up(priority_heap_node_t* heap, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (heap[index].priority <= heap[parent].priority) {
            break;
        }
        heap_swap(heap, index, parent);
        index = parent;
    }
}

/**
 * @brief Sift down in max-heap
 */
static void heap_sift_down(priority_heap_node_t* heap, size_t size, size_t index) {
    while (true) {
        size_t largest = index;
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;

        if (left < size && heap[left].priority > heap[largest].priority) {
            largest = left;
        }
        if (right < size && heap[right].priority > heap[largest].priority) {
            largest = right;
        }

        if (largest == index) {
            break;
        }

        heap_swap(heap, index, largest);
        index = largest;
    }
}

/**
 * @brief Insert into heap
 */
static bool heap_insert(prospective_scheduler_t* scheduler, scheduled_intention_t* item) {
    if (scheduler->heap_size >= scheduler->heap_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "heap_insert: capacity exceeded");
        return false;
    }

    size_t index = scheduler->heap_size;
    scheduler->heap[index].item = item;
    scheduler->heap[index].priority = item->priority;
    item->heap_index = index;
    scheduler->heap_size++;

    heap_sift_up(scheduler->heap, index);
    return true;
}

/**
 * @brief Remove from heap by index
 */
static scheduled_intention_t* heap_remove_at(prospective_scheduler_t* scheduler, size_t index) {
    if (index >= scheduler->heap_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heap_remove_at: capacity exceeded");
        return NULL;
    }

    scheduled_intention_t* removed = scheduler->heap[index].item;

    // Move last element to this position
    scheduler->heap_size--;
    if (index < scheduler->heap_size) {
        scheduler->heap[index] = scheduler->heap[scheduler->heap_size];
        if (scheduler->heap[index].item) {
            scheduler->heap[index].item->heap_index = index;
        }

        // Restore heap property
        if (index > 0) {
            size_t parent = (index - 1) / 2;
            if (scheduler->heap[index].priority > scheduler->heap[parent].priority) {
                heap_sift_up(scheduler->heap, index);
            } else {
                heap_sift_down(scheduler->heap, scheduler->heap_size, index);
            }
        } else {
            heap_sift_down(scheduler->heap, scheduler->heap_size, index);
        }
    }

    return removed;
}

/**
 * @brief Update priority and restore heap property
 */
static void heap_update_priority(prospective_scheduler_t* scheduler, size_t index, float new_priority) {
    if (index >= scheduler->heap_size) {
        return;
    }

    float old_priority = scheduler->heap[index].priority;
    scheduler->heap[index].priority = new_priority;
    if (scheduler->heap[index].item) {
        scheduler->heap[index].item->priority = new_priority;
    }

    if (new_priority > old_priority) {
        heap_sift_up(scheduler->heap, index);
    } else if (new_priority < old_priority) {
        heap_sift_down(scheduler->heap, scheduler->heap_size, index);
    }
}

//=============================================================================
// Internal Helper Functions - Scheduled Intention
//=============================================================================

/**
 * @brief Create scheduled intention wrapper
 */
static scheduled_intention_t* scheduled_intention_create(prospective_intention_t* intention) {
    if (!intention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intention is NULL");

        return NULL;
    }

    scheduled_intention_t* scheduled = (scheduled_intention_t*)nimcp_malloc(sizeof(scheduled_intention_t));
    if (!scheduled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate scheduled");

        return NULL;
    }

    memset(scheduled, 0, sizeof(scheduled_intention_t));
    scheduled->intention = intention;
    scheduled->priority = 0.0f;
    scheduled->time_until_trigger = 0.0f;
    scheduled->urgency_factor = 0.0f;
    scheduled->recency_factor = 1.0f;
    scheduled->reminder_level = REMINDER_NONE;
    scheduled->heap_index = 0;
    scheduled->conflict_group_id = 0;

    // Initialize reminder tracking
    for (int i = 0; i < 6; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 6 > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)6);
        }

        scheduled->reminded_at_level[i] = false;
    }

    return scheduled;
}

/**
 * @brief Destroy scheduled intention wrapper (does NOT destroy intention)
 */
static void scheduled_intention_destroy(scheduled_intention_t* scheduled) {
    if (scheduled) {
        nimcp_free(scheduled);
        scheduled = NULL;
    }
}

/**
 * @brief Find scheduled intention by ID
 */
static scheduled_intention_t* find_scheduled_by_id(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        if (scheduler->all_intentions[i] &&
            scheduler->all_intentions[i]->intention &&
            scheduler->all_intentions[i]->intention->intention_id == intention_id) {
            return scheduler->all_intentions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_scheduled_by_id: operation failed");
    return NULL;
}

//=============================================================================
// Internal Helper Functions - Time-Ordered List
//=============================================================================

/**
 * @brief Insert into time-ordered list (sorted by deadline)
 */
static bool time_ordered_insert(prospective_scheduler_t* scheduler, scheduled_intention_t* item) {
    if (scheduler->num_time_ordered >= scheduler->time_ordered_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "time_ordered_insert: capacity exceeded");
        return false;
    }

    float deadline = item->intention->time_window.end_time;

    // Find insertion point (binary search)
    size_t left = 0;
    size_t right = scheduler->num_time_ordered;
    while (left < right) {
        size_t mid = (left + right) / 2;
        if (scheduler->time_ordered[mid]->intention->time_window.end_time < deadline) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    // Shift elements to make room
    for (size_t i = scheduler->num_time_ordered; i > left; i--) {
        scheduler->time_ordered[i] = scheduler->time_ordered[i - 1];
    }

    scheduler->time_ordered[left] = item;
    scheduler->num_time_ordered++;
    return true;
}

/**
 * @brief Remove from time-ordered list
 */
static bool time_ordered_remove(prospective_scheduler_t* scheduler, scheduled_intention_t* item) {
    for (size_t i = 0; i < scheduler->num_time_ordered; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_time_ordered > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_time_ordered);
        }

        if (scheduler->time_ordered[i] == item) {
            // Shift elements
            for (size_t j = i; j < scheduler->num_time_ordered - 1; j++) {
                scheduler->time_ordered[j] = scheduler->time_ordered[j + 1];
            }
            scheduler->num_time_ordered--;
            return true;
        }
    }
    return false;
}

//=============================================================================
// Internal Helper Functions - Conflict Groups
//=============================================================================

/**
 * @brief Find conflict group containing intention
 */
static pr_conflict_group_t* find_conflict_group_for_intention(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    for (size_t g = 0; g < scheduler->num_conflict_groups; g++) {
        /* Phase 8: Loop progress heartbeat */
        if ((g & 0xFF) == 0 && scheduler->num_conflict_groups > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(g + 1) / (float)scheduler->num_conflict_groups);
        }

        pr_conflict_group_t* group = &scheduler->conflict_groups[g];
        for (size_t i = 0; i < group->num_intentions; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && group->num_intentions > 256) {
                prospective_scheduler_heartbeat("prospective__loop",
                                 (float)(i + 1) / (float)group->num_intentions);
            }

            if (group->intention_ids[i] == intention_id) {
                return group;
            }
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_conflict_group_for_intention: validation failed");
    return NULL;
}

/**
 * @brief Find conflict group by ID
 */
static pr_conflict_group_t* find_conflict_group_by_id(
    prospective_scheduler_t* scheduler,
    uint32_t group_id
) {
    for (size_t i = 0; i < scheduler->num_conflict_groups; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_conflict_groups > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_conflict_groups);
        }

        if (scheduler->conflict_groups[i].group_id == group_id) {
            return &scheduler->conflict_groups[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_conflict_group_by_id: validation failed");
    return NULL;
}

//=============================================================================
// Public API - Configuration
//=============================================================================

prospective_scheduler_config_t prospective_scheduler_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__config_default", 0.0f);


    prospective_scheduler_config_t config = {
        .max_intentions = PR_SCHED_DEFAULT_MAX_INTENTIONS,
        .max_conflict_groups = PR_SCHED_MAX_CONFLICT_GROUPS,

        .reminder_far = PR_SCHED_DEFAULT_REMINDER_FAR,
        .reminder_moderate = PR_SCHED_DEFAULT_REMINDER_MODERATE,
        .reminder_soon = PR_SCHED_DEFAULT_REMINDER_SOON,
        .reminder_imminent = PR_SCHED_DEFAULT_REMINDER_IMMINENT,

        .urgency_weight = PR_SCHED_DEFAULT_URGENCY_WEIGHT,
        .importance_weight = PR_SCHED_DEFAULT_IMPORTANCE_WEIGHT,
        .recency_weight = PR_SCHED_DEFAULT_RECENCY_WEIGHT,

        .urgency_tau = PR_SCHED_URGENCY_TAU,
        .recency_tau = PR_SCHED_RECENCY_TAU,

        .default_conflict_strategy = PR_CONFLICT_HIGHEST_PRIORITY,
        .allow_concurrent = false,
        .max_concurrent = 1,

        .auto_remove_completed = true,
        .auto_remove_expired = false,
        .enable_snooze = true,
        .default_snooze_duration = 300.0f,  // 5 minutes

        .track_statistics = true
    };
    return config;
}

bool prospective_scheduler_config_validate(const prospective_scheduler_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_config_validate: config is NULL");
        return false;
    }

    // Check capacity
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__config_validate", 0.0f);


    if (config->max_intentions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prospective_scheduler_config_validate: config->max_intentions is zero");
        return false;
    }

    // Check reminder ordering: far > moderate > soon > imminent > 0
    if (config->reminder_far <= config->reminder_moderate ||
        config->reminder_moderate <= config->reminder_soon ||
        config->reminder_soon <= config->reminder_imminent ||
        config->reminder_imminent <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prospective_scheduler_config_validate: config->max_intentions is zero");
        return false;
    }

    // Check weights are non-negative
    if (config->urgency_weight < 0 || config->importance_weight < 0 ||
        config->recency_weight < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prospective_scheduler_config_validate: operation failed");
        return false;
    }

    // Check time constants
    if (config->urgency_tau <= 0 || config->recency_tau <= 0) {
        return false;
    }

    return true;
}

//=============================================================================
// Public API - Lifecycle
//=============================================================================

prospective_scheduler_t* prospective_scheduler_create(
    const prospective_scheduler_config_t* config
) {
    // Use defaults if no config
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__create", 0.0f);


    prospective_scheduler_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = prospective_scheduler_config_default();
    }

    // Validate config
    if (!prospective_scheduler_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_create: prospective_scheduler_config_validate is NULL");
        return NULL;
    }

    // Allocate scheduler
    prospective_scheduler_t* scheduler = (prospective_scheduler_t*)nimcp_malloc(
        sizeof(prospective_scheduler_t));
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate scheduler");

        return NULL;
    }
    memset(scheduler, 0, sizeof(prospective_scheduler_t));

    // Store config
    scheduler->config = cfg;

    // Allocate heap
    scheduler->heap_capacity = cfg.max_intentions;
    scheduler->heap = (priority_heap_node_t*)nimcp_calloc(
        cfg.max_intentions, sizeof(priority_heap_node_t));
    if (!scheduler->heap) {
        nimcp_free(scheduler);
        scheduler = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prospective_scheduler_create: scheduler->heap is NULL");
        return NULL;
    }

    // Allocate time-ordered list
    scheduler->time_ordered_capacity = cfg.max_intentions;
    scheduler->time_ordered = (scheduled_intention_t**)nimcp_calloc(
        cfg.max_intentions, sizeof(scheduled_intention_t*));
    if (!scheduler->time_ordered) {
        nimcp_free(scheduler->heap);
        nimcp_free(scheduler);
        scheduler = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prospective_scheduler_create: scheduler->time_ordered is NULL");
        return NULL;
    }

    // Allocate all intentions array
    scheduler->all_intentions = (scheduled_intention_t**)nimcp_calloc(
        cfg.max_intentions, sizeof(scheduled_intention_t*));
    if (!scheduler->all_intentions) {
        nimcp_free(scheduler->time_ordered);
        nimcp_free(scheduler->heap);
        nimcp_free(scheduler);
        scheduler = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_create: scheduler->all_intentions is NULL");
        return NULL;
    }

    // Allocate conflict groups
    scheduler->conflict_groups = (pr_conflict_group_t*)nimcp_calloc(
        cfg.max_conflict_groups, sizeof(pr_conflict_group_t));
    if (!scheduler->conflict_groups) {
        nimcp_free(scheduler->all_intentions);
        nimcp_free(scheduler->time_ordered);
        nimcp_free(scheduler->heap);
        nimcp_free(scheduler);
        scheduler = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_create: scheduler->conflict_groups is NULL");
        return NULL;
    }

    // Initialize state
    scheduler->heap_size = 0;
    scheduler->num_time_ordered = 0;
    scheduler->num_intentions = 0;
    scheduler->num_conflict_groups = 0;
    scheduler->next_conflict_group_id = 1;

    scheduler->reminder_callback = NULL;
    scheduler->reminder_user_data = NULL;
    scheduler->conflict_callback = NULL;
    scheduler->conflict_user_data = NULL;
    scheduler->trigger_callback = NULL;
    scheduler->trigger_user_data = NULL;

    scheduler->current_time = 0.0f;
    scheduler->last_update_ms = get_current_time_ms();
    scheduler->next_intention_id = 1;

    // Initialize statistics
    memset(&scheduler->stats, 0, sizeof(pr_sched_stats_t));
    scheduler->stats.last_reset_time_ms = scheduler->last_update_ms;

    scheduler->initialized = true;

    return scheduler;
}

void prospective_scheduler_destroy(prospective_scheduler_t* scheduler) {
    if (!scheduler) {
        return;
    }

    // Free scheduled intention wrappers (but NOT the intentions themselves)
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__destroy", 0.0f);


    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        if (scheduler->all_intentions[i]) {
            scheduled_intention_destroy(scheduler->all_intentions[i]);
        }
    }

    // Free conflict group arrays
    for (size_t i = 0; i < scheduler->num_conflict_groups; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_conflict_groups > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_conflict_groups);
        }

        if (scheduler->conflict_groups[i].intention_ids) {
            nimcp_free(scheduler->conflict_groups[i].intention_ids);
        }
    }

    // Free main arrays
    if (scheduler->heap) {
        nimcp_free(scheduler->heap);
    }
    if (scheduler->time_ordered) {
        nimcp_free(scheduler->time_ordered);
    }
    if (scheduler->all_intentions) {
        nimcp_free(scheduler->all_intentions);
    }
    if (scheduler->conflict_groups) {
        nimcp_free(scheduler->conflict_groups);
    }

    nimcp_free(scheduler);
    scheduler = NULL;
}

pr_sched_error_t prospective_scheduler_reset(prospective_scheduler_t* scheduler) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    // Free scheduled intention wrappers
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__reset", 0.0f);


    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        if (scheduler->all_intentions[i]) {
            scheduled_intention_destroy(scheduler->all_intentions[i]);
            scheduler->all_intentions[i] = NULL;
        }
    }

    // Clear arrays
    memset(scheduler->heap, 0, scheduler->heap_capacity * sizeof(priority_heap_node_t));
    memset(scheduler->time_ordered, 0, scheduler->time_ordered_capacity * sizeof(scheduled_intention_t*));

    // Free and reset conflict groups
    for (size_t i = 0; i < scheduler->num_conflict_groups; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_conflict_groups > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_conflict_groups);
        }

        if (scheduler->conflict_groups[i].intention_ids) {
            nimcp_free(scheduler->conflict_groups[i].intention_ids);
            scheduler->conflict_groups[i].intention_ids = NULL;
        }
    }
    memset(scheduler->conflict_groups, 0,
           scheduler->config.max_conflict_groups * sizeof(pr_conflict_group_t));

    // Reset counts
    scheduler->heap_size = 0;
    scheduler->num_time_ordered = 0;
    scheduler->num_intentions = 0;
    scheduler->num_conflict_groups = 0;
    scheduler->next_conflict_group_id = 1;

    // Reset statistics
    memset(&scheduler->stats, 0, sizeof(pr_sched_stats_t));
    scheduler->stats.last_reset_time_ms = get_current_time_ms();

    return PR_SCHED_SUCCESS;
}

//=============================================================================
// Public API - Intention Creation
//=============================================================================

prospective_intention_t* prospective_intention_create(
    const char* name,
    const char* description,
    pr_intent_trigger_type_t trigger_type,
    float importance
) {
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__prospective_intentio", 0.0f);


    prospective_intention_t* intention = (prospective_intention_t*)nimcp_malloc(
        sizeof(prospective_intention_t));
    if (!intention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate intention");

        return NULL;
    }

    memset(intention, 0, sizeof(prospective_intention_t));

    // Set identity
    intention->intention_id = 0;  // Will be assigned when added to scheduler
    safe_strcpy(intention->name, name ? name : "Unnamed", sizeof(intention->name));
    safe_strcpy(intention->description, description ? description : "", sizeof(intention->description));

    // Set trigger
    intention->trigger_type = trigger_type;

    // Initialize time window to defaults
    intention->time_window.start_time = 0.0f;
    intention->time_window.end_time = 3600.0f;  // 1 hour default
    intention->time_window.optimal_time = 1800.0f;  // 30 min default
    intention->time_window.is_absolute = false;

    // Set priority factors
    intention->importance = nimcp_clampf(importance, 0.0f, 1.0f);
    intention->urgency_base = 0.5f;
    intention->flexibility = 0.5f;

    // Initialize state
    intention->state = PR_INTENT_STATE_PENDING;
    intention->created_time_ms = get_current_time_ms();
    intention->last_updated_ms = intention->created_time_ms;
    intention->reminder_count = 0;
    intention->snooze_count = 0;

    return intention;
}

void prospective_intention_destroy(prospective_intention_t* intention) {
    if (!intention) {
        return;
    }

    // Note: Does NOT free action_data, user_data, or event_trigger.cue_signature
    // Caller must free those separately

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__prospective_intentio", 0.0f);


    nimcp_free(intention);
    intention = NULL;
}

pr_sched_error_t prospective_intention_set_time_window(
    prospective_intention_t* intention,
    float start_time,
    float end_time,
    float optimal_time,
    bool is_absolute
) {
    if (!intention) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__prospective_intentio", 0.0f);


    if (end_time <= start_time) {
        return PR_SCHED_ERROR_INVALID_TIME;
    }

    if (optimal_time < start_time || optimal_time > end_time) {
        optimal_time = (start_time + end_time) / 2.0f;
    }

    intention->time_window.start_time = start_time;
    intention->time_window.end_time = end_time;
    intention->time_window.optimal_time = optimal_time;
    intention->time_window.is_absolute = is_absolute;
    intention->last_updated_ms = get_current_time_ms();

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_intention_set_event_trigger(
    prospective_intention_t* intention,
    const char* event_type,
    const prime_signature_t* cue_signature,
    float similarity_threshold
) {
    if (!intention) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__prospective_intentio", 0.0f);


    intention->trigger_type = PR_INTENT_TRIGGER_EVENT;
    safe_strcpy(intention->event_trigger.event_type, event_type ? event_type : "",
                sizeof(intention->event_trigger.event_type));
    intention->event_trigger.cue_signature = (prime_signature_t*)cue_signature;  // Not copied
    intention->event_trigger.similarity_threshold = nimcp_clampf(similarity_threshold, 0.0f, 1.0f);
    intention->last_updated_ms = get_current_time_ms();

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_intention_set_activity_trigger(
    prospective_intention_t* intention,
    const char* activity_name,
    uint64_t activity_id,
    bool require_success
) {
    if (!intention) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__prospective_intentio", 0.0f);


    intention->trigger_type = PR_INTENT_TRIGGER_ACTIVITY;
    safe_strcpy(intention->activity_trigger.activity_name, activity_name ? activity_name : "",
                sizeof(intention->activity_trigger.activity_name));
    intention->activity_trigger.activity_id = activity_id;
    intention->activity_trigger.require_success = require_success;
    intention->last_updated_ms = get_current_time_ms();

    return PR_SCHED_SUCCESS;
}

//=============================================================================
// Public API - Scheduling
//=============================================================================

pr_sched_error_t prospective_scheduler_add(
    prospective_scheduler_t* scheduler,
    prospective_intention_t* intention
) {
    if (!scheduler || !intention) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    // Check capacity
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__add", 0.0f);


    if (scheduler->num_intentions >= scheduler->config.max_intentions) {
        return PR_SCHED_ERROR_CAPACITY_EXCEEDED;
    }

    // Assign ID if not set
    if (intention->intention_id == 0) {
        intention->intention_id = scheduler->next_intention_id++;
    } else {
        // Check for duplicate
        if (find_scheduled_by_id(scheduler, intention->intention_id)) {
            return PR_SCHED_ERROR_ALREADY_EXISTS;
        }
    }

    // Create scheduled wrapper
    scheduled_intention_t* scheduled = scheduled_intention_create(intention);
    if (!scheduled) {
        return PR_SCHED_ERROR_NO_MEMORY;
    }

    // Compute initial priority
    float time_until = intention->time_window.end_time - scheduler->current_time;
    scheduled->time_until_trigger = time_until;
    scheduled->urgency_factor = pr_sched_compute_urgency(time_until, scheduler->config.urgency_tau);
    scheduled->recency_factor = 1.0f;  // Just added, full recency
    scheduled->priority = pr_sched_compute_priority(
        scheduled->urgency_factor,
        intention->importance,
        scheduled->recency_factor,
        &scheduler->config
    );

    // Compute initial reminder level
    scheduled->reminder_level = pr_sched_compute_reminder_level(
        time_until, &scheduler->config);

    // Add to all_intentions array
    scheduler->all_intentions[scheduler->num_intentions] = scheduled;
    scheduler->num_intentions++;

    // Insert into heap
    if (!heap_insert(scheduler, scheduled)) {
        // Rollback
        scheduler->num_intentions--;
        scheduled_intention_destroy(scheduled);
        return PR_SCHED_ERROR_NO_MEMORY;
    }

    // Insert into time-ordered list (for time-based intentions)
    if (intention->trigger_type == PR_INTENT_TRIGGER_TIME) {
        time_ordered_insert(scheduler, scheduled);
    }

    // Update statistics
    if (scheduler->config.track_statistics) {
        scheduler->stats.total_added++;
        scheduler->stats.current_pending++;
    }

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_remove(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    // Find scheduled intention
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__remove", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    // Remove from heap
    heap_remove_at(scheduler, scheduled->heap_index);

    // Remove from time-ordered list
    time_ordered_remove(scheduler, scheduled);

    // Remove from all_intentions array
    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        if (scheduler->all_intentions[i] == scheduled) {
            // Shift remaining elements
            for (size_t j = i; j < scheduler->num_intentions - 1; j++) {
                scheduler->all_intentions[j] = scheduler->all_intentions[j + 1];
            }
            scheduler->num_intentions--;
            break;
        }
    }

    // Update statistics
    if (scheduler->config.track_statistics) {
        if (scheduled->intention->state == PR_INTENT_STATE_PENDING) {
            scheduler->stats.current_pending--;
        } else if (scheduled->intention->state == PR_INTENT_STATE_ACTIVE) {
            scheduler->stats.current_active--;
        }
    }

    // Destroy wrapper (not the intention itself)
    scheduled_intention_destroy(scheduled);

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_update(
    prospective_scheduler_t* scheduler,
    float current_time
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__update", 0.0f);


    scheduler->current_time = current_time;
    scheduler->last_update_ms = get_current_time_ms();

    // Update all scheduled intentions
    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        prospective_intention_t* intent = scheduled->intention;

        // Skip non-pending intentions
        if (intent->state != PR_INTENT_STATE_PENDING) {
            continue;
        }

        // Update time until trigger
        float deadline = intent->time_window.end_time;
        if (!intent->time_window.is_absolute) {
            // Relative time: convert based on creation
            float elapsed_since_create = (float)(scheduler->last_update_ms - intent->created_time_ms) / 1000.0f;
            scheduled->time_until_trigger = deadline - elapsed_since_create;
        } else {
            scheduled->time_until_trigger = deadline - current_time;
        }

        // Update urgency
        scheduled->urgency_factor = pr_sched_compute_urgency(
            scheduled->time_until_trigger,
            scheduler->config.urgency_tau
        );

        // Update recency (decay over time)
        float age_seconds = (float)(scheduler->last_update_ms - intent->created_time_ms) / 1000.0f;
        scheduled->recency_factor = expf(-age_seconds / scheduler->config.recency_tau);

        // Recompute priority
        float new_priority = pr_sched_compute_priority(
            scheduled->urgency_factor,
            intent->importance,
            scheduled->recency_factor,
            &scheduler->config
        );

        // Update reminder level
        reminder_level_t new_level = pr_sched_compute_reminder_level(
            scheduled->time_until_trigger,
            &scheduler->config
        );
        scheduled->reminder_level = new_level;

        // Update heap with new priority
        heap_update_priority(scheduler, scheduled->heap_index, new_priority);

        // Check if expired
        if (scheduled->time_until_trigger < 0 &&
            scheduler->config.auto_remove_expired &&
            new_level == REMINDER_OVERDUE) {
            // Mark as expired
            intent->state = PR_INTENT_STATE_EXPIRED;
            if (scheduler->config.track_statistics) {
                scheduler->stats.total_expired++;
                scheduler->stats.current_pending--;
            }
        }

        intent->last_updated_ms = scheduler->last_update_ms;
    }

    return PR_SCHED_SUCCESS;
}

prospective_intention_t* prospective_scheduler_get_next(
    prospective_scheduler_t* scheduler
) {
    if (!scheduler || scheduler->heap_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prospective_scheduler_get_next: scheduler is NULL");
        return NULL;
    }

    // Get and remove top of heap
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_next", 0.0f);


    scheduled_intention_t* scheduled = heap_remove_at(scheduler, 0);
    if (!scheduled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scheduled is NULL");

        return NULL;
    }

    prospective_intention_t* intention = scheduled->intention;

    // Remove from time-ordered list
    time_ordered_remove(scheduler, scheduled);

    // Remove from all_intentions array
    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        if (scheduler->all_intentions[i] == scheduled) {
            for (size_t j = i; j < scheduler->num_intentions - 1; j++) {
                scheduler->all_intentions[j] = scheduler->all_intentions[j + 1];
            }
            scheduler->num_intentions--;
            break;
        }
    }

    // Update statistics
    if (scheduler->config.track_statistics) {
        scheduler->stats.current_pending--;
    }

    // Destroy wrapper
    scheduled_intention_destroy(scheduled);

    return intention;
}

prospective_intention_t* prospective_scheduler_peek(
    const prospective_scheduler_t* scheduler
) {
    if (!scheduler || scheduler->heap_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prospective_scheduler_peek: scheduler is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__peek", 0.0f);


    if (scheduler->heap[0].item && scheduler->heap[0].item->intention) {
        return scheduler->heap[0].item->intention;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_peek: validation failed");
    return NULL;
}

int prospective_scheduler_get_due(
    prospective_scheduler_t* scheduler,
    prospective_intention_t** out_intentions,
    size_t max_count
) {
    if (!scheduler || !out_intentions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_get_due: required parameter is NULL (scheduler, out_intentions)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_due", 0.0f);


    int count = 0;
    for (size_t i = 0; i < scheduler->num_intentions && (size_t)count < max_count; i++) {
        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        if (scheduled->intention->state == PR_INTENT_STATE_PENDING &&
            scheduled->time_until_trigger <= 0) {
            out_intentions[count++] = scheduled->intention;
        }
    }

    return count;
}

int prospective_scheduler_get_in_window(
    const prospective_scheduler_t* scheduler,
    float time_start,
    float time_end,
    prospective_intention_t** out_intentions,
    size_t max_count
) {
    if (!scheduler || !out_intentions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_get_in_window: required parameter is NULL (scheduler, out_intentions)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_in_window", 0.0f);


    int count = 0;
    for (size_t i = 0; i < scheduler->num_intentions && (size_t)count < max_count; i++) {
        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        prospective_intention_t* intent = scheduled->intention;
        if (intent->state != PR_INTENT_STATE_PENDING) {
            continue;
        }

        // Check if deadline falls within window
        float deadline = intent->time_window.end_time;
        if (!intent->time_window.is_absolute) {
            float age = (float)(scheduler->last_update_ms - intent->created_time_ms) / 1000.0f;
            float remaining = deadline - age;
            float abs_deadline = scheduler->current_time + remaining;
            if (abs_deadline >= time_start && abs_deadline <= time_end) {
                out_intentions[count++] = intent;
            }
        } else {
            if (deadline >= time_start && deadline <= time_end) {
                out_intentions[count++] = intent;
            }
        }
    }

    return count;
}

//=============================================================================
// Public API - Reminders
//=============================================================================

int prospective_scheduler_check_reminders(prospective_scheduler_t* scheduler) {
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scheduler is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__check_reminders", 0.0f);


    int reminders_fired = 0;

    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        if (scheduled->intention->state != PR_INTENT_STATE_PENDING) {
            continue;
        }

        reminder_level_t level = scheduled->reminder_level;

        // Check if we need to fire reminder at this level
        if (level != REMINDER_NONE && !scheduled->reminded_at_level[level]) {
            // Mark as reminded at this level
            scheduled->reminded_at_level[level] = true;
            scheduled->intention->reminder_count++;

            // Fire callback if set
            if (scheduler->reminder_callback) {
                scheduler->reminder_callback(
                    scheduled->intention,
                    level,
                    scheduler->reminder_user_data
                );
            }

            reminders_fired++;

            // Update statistics
            if (scheduler->config.track_statistics) {
                scheduler->stats.reminders_sent++;
            }
        }
    }

    return reminders_fired;
}

pr_sched_error_t prospective_scheduler_set_reminder_callback(
    prospective_scheduler_t* scheduler,
    pr_reminder_callback_t callback,
    void* user_data
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__set_reminder_callbac", 0.0f);


    scheduler->reminder_callback = callback;
    scheduler->reminder_user_data = user_data;

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_snooze(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float snooze_duration
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    if (!scheduler->config.enable_snooze) {
        return PR_SCHED_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__snooze", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    // Use default duration if not specified
    if (snooze_duration <= 0) {
        snooze_duration = scheduler->config.default_snooze_duration;
    }

    // Reset reminded flags (will re-trigger at appropriate level after snooze)
    for (int i = 0; i < 6; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 6 > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)6);
        }

        scheduled->reminded_at_level[i] = false;
    }

    // Update snooze count
    scheduled->intention->snooze_count++;

    // Update statistics
    if (scheduler->config.track_statistics) {
        scheduler->stats.reminders_snoozed++;
    }

    return PR_SCHED_SUCCESS;
}

reminder_level_t prospective_scheduler_get_reminder_level(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    if (!scheduler) {
        return REMINDER_NONE;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_reminder_level", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(
        (prospective_scheduler_t*)scheduler, intention_id);
    if (!scheduled) {
        return REMINDER_NONE;
    }

    return scheduled->reminder_level;
}

//=============================================================================
// Public API - Conflict Management
//=============================================================================

uint32_t prospective_scheduler_add_conflict(
    prospective_scheduler_t* scheduler,
    const uint64_t* intention_ids,
    size_t num_intentions,
    pr_conflict_strategy_t strategy
) {
    if (!scheduler || !intention_ids || num_intentions < 2) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__add_conflict", 0.0f);


    if (scheduler->num_conflict_groups >= scheduler->config.max_conflict_groups) {
        return 0;
    }

    // Create new conflict group
    pr_conflict_group_t* group = &scheduler->conflict_groups[scheduler->num_conflict_groups];

    group->group_id = scheduler->next_conflict_group_id++;
    group->strategy = strategy;
    group->capacity = num_intentions > INITIAL_CONFLICT_CAPACITY ?
                      num_intentions : INITIAL_CONFLICT_CAPACITY;
    group->intention_ids = (uint64_t*)nimcp_calloc(group->capacity, sizeof(uint64_t));
    if (!group->intention_ids) {
        return 0;
    }

    // Copy intention IDs and update scheduled intentions
    group->num_intentions = 0;
    for (size_t i = 0; i < num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)num_intentions);
        }

        scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_ids[i]);
        if (scheduled) {
            group->intention_ids[group->num_intentions++] = intention_ids[i];
            scheduled->conflict_group_id = group->group_id;
        }
    }

    if (group->num_intentions < 2) {
        // Not enough valid intentions for a conflict
        nimcp_free(group->intention_ids);
        group->intention_ids = NULL;
        return 0;
    }

    scheduler->num_conflict_groups++;

    return group->group_id;
}

pr_sched_error_t prospective_scheduler_remove_conflict(
    prospective_scheduler_t* scheduler,
    uint32_t group_id
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    // Find the group
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__remove_conflict", 0.0f);


    pr_conflict_group_t* group = find_conflict_group_by_id(scheduler, group_id);
    if (!group) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    // Clear conflict_group_id from scheduled intentions
    for (size_t i = 0; i < group->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && group->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)group->num_intentions);
        }

        scheduled_intention_t* scheduled = find_scheduled_by_id(
            scheduler, group->intention_ids[i]);
        if (scheduled) {
            scheduled->conflict_group_id = 0;
        }
    }

    // Free group resources
    if (group->intention_ids) {
        nimcp_free(group->intention_ids);
    }

    // Shift remaining groups
    size_t group_index = group - scheduler->conflict_groups;
    for (size_t i = group_index; i < scheduler->num_conflict_groups - 1; i++) {
        scheduler->conflict_groups[i] = scheduler->conflict_groups[i + 1];
    }
    scheduler->num_conflict_groups--;

    // Clear the last slot
    memset(&scheduler->conflict_groups[scheduler->num_conflict_groups], 0,
           sizeof(pr_conflict_group_t));

    return PR_SCHED_SUCCESS;
}

uint64_t prospective_scheduler_resolve_conflict(
    prospective_scheduler_t* scheduler,
    uint32_t group_id
) {
    if (!scheduler) {
        return PR_SCHED_INVALID_ID;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__resolve_conflict", 0.0f);


    pr_conflict_group_t* group = find_conflict_group_by_id(scheduler, group_id);
    if (!group || group->num_intentions == 0) {
        return PR_SCHED_INVALID_ID;
    }

    // Update statistics
    if (scheduler->config.track_statistics) {
        scheduler->stats.conflicts_detected++;
    }

    switch (group->strategy) {
        case PR_CONFLICT_HIGHEST_PRIORITY: {
            // Find highest priority intention in group
            uint64_t best_id = PR_SCHED_INVALID_ID;
            float best_priority = -1.0f;

            for (size_t i = 0; i < group->num_intentions; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && group->num_intentions > 256) {
                    prospective_scheduler_heartbeat("prospective__loop",
                                     (float)(i + 1) / (float)group->num_intentions);
                }

                scheduled_intention_t* scheduled = find_scheduled_by_id(
                    scheduler, group->intention_ids[i]);
                if (scheduled && scheduled->priority > best_priority) {
                    best_priority = scheduled->priority;
                    best_id = group->intention_ids[i];
                }
            }

            if (scheduler->config.track_statistics) {
                scheduler->stats.conflicts_resolved++;
            }
            return best_id;
        }

        case PR_CONFLICT_SEQUENTIAL: {
            // Return first by deadline
            uint64_t first_id = PR_SCHED_INVALID_ID;
            float earliest_deadline = 1e10f;

            for (size_t i = 0; i < group->num_intentions; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && group->num_intentions > 256) {
                    prospective_scheduler_heartbeat("prospective__loop",
                                     (float)(i + 1) / (float)group->num_intentions);
                }

                scheduled_intention_t* scheduled = find_scheduled_by_id(
                    scheduler, group->intention_ids[i]);
                if (scheduled && scheduled->intention) {
                    float deadline = scheduled->intention->time_window.end_time;
                    if (deadline < earliest_deadline) {
                        earliest_deadline = deadline;
                        first_id = group->intention_ids[i];
                    }
                }
            }

            if (scheduler->config.track_statistics) {
                scheduler->stats.conflicts_resolved++;
            }
            return first_id;
        }

        case PR_CONFLICT_USER_CHOICE: {
            // Fire callback
            if (scheduler->conflict_callback) {
                // Build array of intention pointers
                prospective_intention_t* intentions[PR_SCHED_MAX_CONFLICTS_PER_GROUP];
                size_t count = 0;

                for (size_t i = 0; i < group->num_intentions && count < PR_SCHED_MAX_CONFLICTS_PER_GROUP; i++) {
                    scheduled_intention_t* scheduled = find_scheduled_by_id(
                        scheduler, group->intention_ids[i]);
                    if (scheduled && scheduled->intention) {
                        intentions[count++] = scheduled->intention;
                    }
                }

                int choice = scheduler->conflict_callback(
                    intentions, count, scheduler->conflict_user_data);

                if (choice >= 0 && (size_t)choice < count) {
                    if (scheduler->config.track_statistics) {
                        scheduler->stats.conflicts_resolved++;
                    }
                    return intentions[choice]->intention_id;
                }
            }
            return PR_SCHED_INVALID_ID;
        }

        case PR_CONFLICT_BATCH:
        default:
            // For BATCH, just return highest priority for now
            return prospective_scheduler_resolve_conflict(scheduler, group_id);
    }
}

pr_sched_error_t prospective_scheduler_set_conflict_callback(
    prospective_scheduler_t* scheduler,
    pr_conflict_callback_t callback,
    void* user_data
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__set_conflict_callbac", 0.0f);


    scheduler->conflict_callback = callback;
    scheduler->conflict_user_data = user_data;

    return PR_SCHED_SUCCESS;
}

bool prospective_scheduler_has_conflict(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    if (!scheduler) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__has_conflict", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(
        (prospective_scheduler_t*)scheduler, intention_id);
    if (!scheduled) {
        return false;
    }

    return scheduled->conflict_group_id != 0;
}

//=============================================================================
// Public API - Rescheduling
//=============================================================================

pr_sched_error_t prospective_scheduler_reschedule(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float new_start,
    float new_end,
    float new_optimal
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__reschedule", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    // Update time window
    pr_sched_error_t err = prospective_intention_set_time_window(
        scheduled->intention, new_start, new_end, new_optimal,
        scheduled->intention->time_window.is_absolute
    );
    if (err != PR_SCHED_SUCCESS) {
        return err;
    }

    // Remove and re-add to time-ordered list
    time_ordered_remove(scheduler, scheduled);
    time_ordered_insert(scheduler, scheduled);

    // Reset reminder tracking
    for (int i = 0; i < 6; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 6 > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)6);
        }

        scheduled->reminded_at_level[i] = false;
    }

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_update_importance(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float new_importance
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__update_importance", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    scheduled->intention->importance = nimcp_clampf(new_importance, 0.0f, 1.0f);
    scheduled->intention->last_updated_ms = get_current_time_ms();

    // Recompute priority
    float new_priority = pr_sched_compute_priority(
        scheduled->urgency_factor,
        scheduled->intention->importance,
        scheduled->recency_factor,
        &scheduler->config
    );

    heap_update_priority(scheduler, scheduled->heap_index, new_priority);

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_mark_completed(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    float quality
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__mark_completed", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    prospective_intention_t* intent = scheduled->intention;
    intent->state = PR_INTENT_STATE_COMPLETED;
    intent->completed_time_ms = get_current_time_ms();
    intent->completion_quality = nimcp_clampf(quality, 0.0f, 1.0f);

    // Update statistics
    if (scheduler->config.track_statistics) {
        scheduler->stats.total_completed++;
        if (intent->state == PR_INTENT_STATE_PENDING) {
            scheduler->stats.current_pending--;
        } else if (intent->state == PR_INTENT_STATE_ACTIVE) {
            scheduler->stats.current_active--;
        }

        // Track timing
        float response_time = (float)(intent->completed_time_ms - intent->triggered_time_ms);
        if (intent->triggered_time_ms > 0 && response_time > 0) {
            scheduler->stats.avg_response_time_ms =
                (scheduler->stats.avg_response_time_ms * (scheduler->stats.total_completed - 1) +
                 response_time) / scheduler->stats.total_completed;
        }
    }

    // Auto-remove if configured
    if (scheduler->config.auto_remove_completed) {
        return prospective_scheduler_remove(scheduler, intention_id);
    }

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_mark_failed(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__mark_failed", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    scheduled->intention->state = PR_INTENT_STATE_FAILED;

    // Update statistics
    if (scheduler->config.track_statistics) {
        scheduler->stats.total_failed++;
        scheduler->stats.current_pending--;
    }

    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_cancel(
    prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__cancel", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    scheduled->intention->state = PR_INTENT_STATE_CANCELLED;

    // Update statistics
    if (scheduler->config.track_statistics) {
        scheduler->stats.total_cancelled++;
        scheduler->stats.current_pending--;
    }

    // Remove from scheduler
    return prospective_scheduler_remove(scheduler, intention_id);
}

//=============================================================================
// Public API - Timeline and Load
//=============================================================================

int prospective_scheduler_get_timeline(
    const prospective_scheduler_t* scheduler,
    float time_horizon,
    pr_timeline_entry_t* out_entries,
    size_t max_entries
) {
    if (!scheduler || !out_entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_get_timeline: required parameter is NULL (scheduler, out_entries)");
        return -1;
    }

    // Collect entries for all pending intentions within horizon
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_timeline", 0.0f);


    size_t count = 0;
    float horizon_end = scheduler->current_time + time_horizon;

    for (size_t i = 0; i < scheduler->num_intentions && count < max_entries; i++) {
        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        prospective_intention_t* intent = scheduled->intention;
        if (intent->state != PR_INTENT_STATE_PENDING) {
            continue;
        }

        // Get absolute times
        float start = intent->time_window.start_time;
        float end = intent->time_window.end_time;

        if (!intent->time_window.is_absolute) {
            float age = (float)(scheduler->last_update_ms - intent->created_time_ms) / 1000.0f;
            start = scheduler->current_time + (start - age);
            end = scheduler->current_time + (end - age);
        }

        // Check if within horizon
        if (end > scheduler->current_time && start < horizon_end) {
            pr_timeline_entry_t* entry = &out_entries[count++];
            entry->intention_id = intent->intention_id;
            safe_strcpy(entry->name, intent->name, sizeof(entry->name));
            entry->start_time = start;
            entry->end_time = end;
            entry->priority = scheduled->priority;
            entry->reminder_level = scheduled->reminder_level;
            entry->has_conflict = (scheduled->conflict_group_id != 0);
        }
    }

    // Sort by start time (simple bubble sort for small N)
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)count);
        }

        for (size_t j = i + 1; j < count; j++) {
            if (out_entries[j].start_time < out_entries[i].start_time) {
                pr_timeline_entry_t temp = out_entries[i];
                out_entries[i] = out_entries[j];
                out_entries[j] = temp;
            }
        }
    }

    return (int)count;
}

pr_sched_error_t prospective_scheduler_estimate_load(
    const prospective_scheduler_t* scheduler,
    float time_horizon,
    pr_cognitive_load_t* out_load
) {
    if (!scheduler || !out_load) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__estimate_load", 0.0f);


    memset(out_load, 0, sizeof(pr_cognitive_load_t));

    float horizon_end = scheduler->current_time + time_horizon;
    float priority_sum = 0.0f;
    float peak_priority = 0.0f;
    float peak_time = 0.0f;
    size_t num_in_horizon = 0;

    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        prospective_intention_t* intent = scheduled->intention;
        if (intent->state != PR_INTENT_STATE_PENDING) {
            continue;
        }

        out_load->num_pending++;

        // Check if imminent
        if (scheduled->reminder_level == REMINDER_IMMINENT ||
            scheduled->reminder_level == REMINDER_OVERDUE) {
            out_load->num_imminent++;
        }

        // Check if has conflict
        if (scheduled->conflict_group_id != 0) {
            out_load->num_conflicts++;
        }

        // Check if within horizon
        float deadline = intent->time_window.end_time;
        if (!intent->time_window.is_absolute) {
            float age = (float)(scheduler->last_update_ms - intent->created_time_ms) / 1000.0f;
            deadline = scheduler->current_time + (deadline - age);
        }

        if (deadline <= horizon_end) {
            num_in_horizon++;
            priority_sum += scheduled->priority;

            if (scheduled->priority > peak_priority) {
                peak_priority = scheduled->priority;
                peak_time = deadline;
            }
        }
    }

    // Compute average priority
    if (out_load->num_pending > 0) {
        out_load->avg_priority = priority_sum / (float)num_in_horizon;
    }

    // Compute total load (normalized)
    // Load = weighted sum of factors
    float pending_factor = (float)out_load->num_pending / (float)scheduler->config.max_intentions;
    float imminent_factor = (float)out_load->num_imminent / fmaxf((float)out_load->num_pending, 1.0f);
    float conflict_factor = (float)out_load->num_conflicts / fmaxf((float)out_load->num_pending, 1.0f);
    float priority_factor = out_load->avg_priority;

    out_load->total_load = nimcp_clampf(
        0.3f * pending_factor +
        0.3f * imminent_factor +
        0.2f * conflict_factor +
        0.2f * priority_factor,
        0.0f, 1.0f
    );

    out_load->peak_load_time = peak_time;

    return PR_SCHED_SUCCESS;
}

bool prospective_scheduler_has_capacity(const prospective_scheduler_t* scheduler) {
    if (!scheduler) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__has_capacity", 0.0f);


    return scheduler->num_intentions < scheduler->config.max_intentions;
}

size_t prospective_scheduler_get_count(const prospective_scheduler_t* scheduler) {
    if (!scheduler) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_count", 0.0f);


    return scheduler->num_intentions;
}

//=============================================================================
// Public API - Event Notification
//=============================================================================

int prospective_scheduler_notify_event(
    prospective_scheduler_t* scheduler,
    const char* event_type,
    const prime_signature_t* event_signature
) {
    if (!scheduler || !event_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_scheduler_notify_event: required parameter is NULL (scheduler, event_type)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__notify_event", 0.0f);


    int triggered_count = 0;

    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        prospective_intention_t* intent = scheduled->intention;

        // Only process pending event-based intentions
        if (intent->state != PR_INTENT_STATE_PENDING ||
            intent->trigger_type != PR_INTENT_TRIGGER_EVENT) {
            continue;
        }

        // Check event type match
        if (strcmp(intent->event_trigger.event_type, event_type) != 0) {
            continue;
        }

        // Check signature similarity if both signatures present
        bool signature_match = true;
        if (intent->event_trigger.cue_signature && event_signature) {
            float similarity = prime_sig_similarity(
                intent->event_trigger.cue_signature,
                event_signature,
                PRIME_SIG_SIMILARITY_COSINE
            );
            signature_match = (similarity >= intent->event_trigger.similarity_threshold);
        }

        if (signature_match) {
            // Trigger the intention
            intent->state = PR_INTENT_STATE_ACTIVE;
            intent->triggered_time_ms = get_current_time_ms();

            // Fire trigger callback
            if (scheduler->trigger_callback) {
                scheduler->trigger_callback(intent, scheduler->trigger_user_data);
            }

            triggered_count++;

            // Update statistics
            if (scheduler->config.track_statistics) {
                scheduler->stats.current_pending--;
                scheduler->stats.current_active++;
            }
        }
    }

    return triggered_count;
}

int prospective_scheduler_notify_activity(
    prospective_scheduler_t* scheduler,
    uint64_t activity_id,
    bool success
) {
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scheduler is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__notify_activity", 0.0f);


    int triggered_count = 0;

    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        scheduled_intention_t* scheduled = scheduler->all_intentions[i];
        if (!scheduled || !scheduled->intention) {
            continue;
        }

        prospective_intention_t* intent = scheduled->intention;

        // Only process pending activity-based intentions
        if (intent->state != PR_INTENT_STATE_PENDING ||
            intent->trigger_type != PR_INTENT_TRIGGER_ACTIVITY) {
            continue;
        }

        // Check activity ID match
        if (intent->activity_trigger.activity_id != activity_id) {
            continue;
        }

        // Check success requirement
        if (intent->activity_trigger.require_success && !success) {
            continue;
        }

        // Trigger the intention
        intent->state = PR_INTENT_STATE_ACTIVE;
        intent->triggered_time_ms = get_current_time_ms();

        // Fire trigger callback
        if (scheduler->trigger_callback) {
            scheduler->trigger_callback(intent, scheduler->trigger_user_data);
        }

        triggered_count++;

        // Update statistics
        if (scheduler->config.track_statistics) {
            scheduler->stats.current_pending--;
            scheduler->stats.current_active++;
        }
    }

    return triggered_count;
}

pr_sched_error_t prospective_scheduler_set_trigger_callback(
    prospective_scheduler_t* scheduler,
    pr_trigger_callback_t callback,
    void* user_data
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__set_trigger_callback", 0.0f);


    scheduler->trigger_callback = callback;
    scheduler->trigger_user_data = user_data;

    return PR_SCHED_SUCCESS;
}

//=============================================================================
// Public API - Query Functions
//=============================================================================

prospective_intention_t* prospective_scheduler_find(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scheduler is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__find", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(
        (prospective_scheduler_t*)scheduler, intention_id);
    if (!scheduled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scheduled is NULL");

        return NULL;
    }

    return scheduled->intention;
}

pr_sched_error_t prospective_scheduler_get_scheduled_info(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id,
    scheduled_intention_t* out_scheduled
) {
    if (!scheduler || !out_scheduled) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_scheduled_info", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(
        (prospective_scheduler_t*)scheduler, intention_id);
    if (!scheduled) {
        return PR_SCHED_ERROR_NOT_FOUND;
    }

    *out_scheduled = *scheduled;
    return PR_SCHED_SUCCESS;
}

float prospective_scheduler_get_priority(
    const prospective_scheduler_t* scheduler,
    uint64_t intention_id
) {
    if (!scheduler) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_priority", 0.0f);


    scheduled_intention_t* scheduled = find_scheduled_by_id(
        (prospective_scheduler_t*)scheduler, intention_id);
    if (!scheduled) {
        return -1.0f;
    }

    return scheduled->priority;
}

//=============================================================================
// Public API - Statistics
//=============================================================================

pr_sched_error_t prospective_scheduler_get_stats(
    const prospective_scheduler_t* scheduler,
    pr_sched_stats_t* out_stats
) {
    if (!scheduler || !out_stats) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    *out_stats = scheduler->stats;
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__get_stats", 0.0f);


    return PR_SCHED_SUCCESS;
}

pr_sched_error_t prospective_scheduler_reset_stats(
    prospective_scheduler_t* scheduler
) {
    if (!scheduler) {
        return PR_SCHED_ERROR_NULL_POINTER;
    }

    // Keep current counts, reset historical
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__reset_stats", 0.0f);


    size_t current_pending = scheduler->stats.current_pending;
    size_t current_active = scheduler->stats.current_active;

    memset(&scheduler->stats, 0, sizeof(pr_sched_stats_t));

    scheduler->stats.current_pending = current_pending;
    scheduler->stats.current_active = current_active;
    scheduler->stats.last_reset_time_ms = get_current_time_ms();

    return PR_SCHED_SUCCESS;
}

void prospective_scheduler_print_state(const prospective_scheduler_t* scheduler) {
    if (!scheduler) {
        printf("Scheduler: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__print_state", 0.0f);


    printf("=== Prospective Scheduler State ===\n");
    printf("Intentions: %zu / %zu\n", scheduler->num_intentions,
           scheduler->config.max_intentions);
    printf("Heap size: %zu / %zu\n", scheduler->heap_size, scheduler->heap_capacity);
    printf("Time-ordered: %zu\n", scheduler->num_time_ordered);
    printf("Conflict groups: %zu\n", scheduler->num_conflict_groups);
    printf("Current time: %.2f\n", scheduler->current_time);
    printf("\n");

    printf("Scheduled Intentions:\n");
    for (size_t i = 0; i < scheduler->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scheduler->num_intentions > 256) {
            prospective_scheduler_heartbeat("prospective__loop",
                             (float)(i + 1) / (float)scheduler->num_intentions);
        }

        scheduled_intention_t* s = scheduler->all_intentions[i];
        if (!s || !s->intention) continue;

        printf("  [%lu] %s\n", (unsigned long)s->intention->intention_id,
               s->intention->name);
        printf("       Priority: %.3f, Time until: %.1f, Reminder: %s\n",
               s->priority, s->time_until_trigger,
               pr_reminder_level_name(s->reminder_level));
        printf("       State: %s, Importance: %.2f\n",
               pr_intent_state_name(s->intention->state),
               s->intention->importance);
    }
    printf("\n");

    if (scheduler->config.track_statistics) {
        printf("Statistics:\n");
        printf("  Total added: %lu, Completed: %lu, Failed: %lu\n",
               (unsigned long)scheduler->stats.total_added,
               (unsigned long)scheduler->stats.total_completed,
               (unsigned long)scheduler->stats.total_failed);
        printf("  Expired: %lu, Cancelled: %lu\n",
               (unsigned long)scheduler->stats.total_expired,
               (unsigned long)scheduler->stats.total_cancelled);
        printf("  Reminders sent: %lu, Snoozed: %lu\n",
               (unsigned long)scheduler->stats.reminders_sent,
               (unsigned long)scheduler->stats.reminders_snoozed);
    }
}

//=============================================================================
// Public API - Utility Functions
//=============================================================================

const char* pr_sched_error_string(pr_sched_error_t error) {
    switch (error) {
        case PR_SCHED_SUCCESS:            return "Success";
        case PR_SCHED_ERROR_NULL_POINTER: return "Null pointer";
        case PR_SCHED_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_SCHED_ERROR_NO_MEMORY:    return "Out of memory";
        case PR_SCHED_ERROR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case PR_SCHED_ERROR_NOT_FOUND:    return "Not found";
        case PR_SCHED_ERROR_INVALID_STATE: return "Invalid state";
        case PR_SCHED_ERROR_CONFLICT:     return "Conflict detected";
        case PR_SCHED_ERROR_ALREADY_EXISTS: return "Already exists";
        case PR_SCHED_ERROR_INVALID_TIME: return "Invalid time";
        case PR_SCHED_ERROR_EMPTY:        return "Scheduler empty";
        case PR_SCHED_ERROR_CALLBACK_FAILED: return "Callback failed";
        default:                          return "Unknown error";
    }
}

const char* pr_reminder_level_name(reminder_level_t level) {
    switch (level) {
        case REMINDER_NONE:      return "NONE";
        case REMINDER_FAR:       return "FAR";
        case REMINDER_MODERATE:  return "MODERATE";
        case REMINDER_SOON:      return "SOON";
        case REMINDER_IMMINENT:  return "IMMINENT";
        case REMINDER_OVERDUE:   return "OVERDUE";
        default:                 return "UNKNOWN";
    }
}

const char* pr_trigger_type_name(pr_intent_trigger_type_t type) {
    switch (type) {
        case PR_INTENT_TRIGGER_TIME:     return "TIME";
        case PR_INTENT_TRIGGER_EVENT:    return "EVENT";
        case PR_INTENT_TRIGGER_ACTIVITY: return "ACTIVITY";
        case PR_INTENT_TRIGGER_LOCATION: return "LOCATION";
        case PR_INTENT_TRIGGER_CONTEXT:  return "CONTEXT";
        default:                         return "UNKNOWN";
    }
}

const char* pr_intent_state_name(pr_intent_state_t state) {
    switch (state) {
        case PR_INTENT_STATE_PENDING:   return "PENDING";
        case PR_INTENT_STATE_ACTIVE:    return "ACTIVE";
        case PR_INTENT_STATE_COMPLETED: return "COMPLETED";
        case PR_INTENT_STATE_FAILED:    return "FAILED";
        case PR_INTENT_STATE_CANCELLED: return "CANCELLED";
        case PR_INTENT_STATE_EXPIRED:   return "EXPIRED";
        default:                        return "UNKNOWN";
    }
}

const char* pr_conflict_strategy_name(pr_conflict_strategy_t strategy) {
    switch (strategy) {
        case PR_CONFLICT_HIGHEST_PRIORITY: return "HIGHEST_PRIORITY";
        case PR_CONFLICT_SEQUENTIAL:       return "SEQUENTIAL";
        case PR_CONFLICT_BATCH:            return "BATCH";
        case PR_CONFLICT_USER_CHOICE:      return "USER_CHOICE";
        default:                           return "UNKNOWN";
    }
}

uint64_t pr_sched_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__pr_sched_current_tim", 0.0f);


    return get_current_time_ms();
}

float pr_sched_compute_priority(
    float urgency,
    float importance,
    float recency,
    const prospective_scheduler_config_t* config
) {
    if (!config) {
        // Use default weights
        return 0.4f * urgency + 0.4f * importance + 0.2f * recency;
    }

    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__pr_sched_compute_pri", 0.0f);


    return config->urgency_weight * urgency +
           config->importance_weight * importance +
           config->recency_weight * recency;
}

float pr_sched_compute_urgency(float time_until_trigger, float urgency_tau) {
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__pr_sched_compute_urg", 0.0f);


    if (urgency_tau <= 0) {
        urgency_tau = PR_SCHED_URGENCY_TAU;
    }

    if (time_until_trigger <= 0) {
        // Overdue: urgency increases beyond 1.0
        return 1.0f + (1.0f - expf(time_until_trigger / urgency_tau));
    }

    // Exponential approach to 1.0 as deadline approaches
    return 1.0f - expf(-1.0f / (time_until_trigger / urgency_tau + 0.01f));
}

reminder_level_t pr_sched_compute_reminder_level(
    float time_until_trigger,
    const prospective_scheduler_config_t* config
) {
    if (!config) {
        // Use defaults
        if (time_until_trigger <= 0) return REMINDER_OVERDUE;
        if (time_until_trigger <= PR_SCHED_DEFAULT_REMINDER_IMMINENT) return REMINDER_IMMINENT;
        if (time_until_trigger <= PR_SCHED_DEFAULT_REMINDER_SOON) return REMINDER_SOON;
        if (time_until_trigger <= PR_SCHED_DEFAULT_REMINDER_MODERATE) return REMINDER_MODERATE;
        if (time_until_trigger <= PR_SCHED_DEFAULT_REMINDER_FAR) return REMINDER_FAR;
        return REMINDER_NONE;
    }

    if (time_until_trigger <= 0) return REMINDER_OVERDUE;
    if (time_until_trigger <= config->reminder_imminent) return REMINDER_IMMINENT;
    if (time_until_trigger <= config->reminder_soon) return REMINDER_SOON;
    if (time_until_trigger <= config->reminder_moderate) return REMINDER_MODERATE;
    if (time_until_trigger <= config->reminder_far) return REMINDER_FAR;
    /* Phase 8: Heartbeat at operation start */
    prospective_scheduler_heartbeat("prospective__pr_sched_compute_rem", 0.0f);


    return REMINDER_NONE;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void prospective_scheduler_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_prospective_scheduler_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int prospective_scheduler_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prospective_scheduler_training_begin: NULL argument");
        return -1;
    }
    prospective_scheduler_heartbeat_instance(NULL, "prospective_scheduler_training_begin", 0.0f);
    return 0;
}

int prospective_scheduler_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prospective_scheduler_training_end: NULL argument");
        return -1;
    }
    prospective_scheduler_heartbeat_instance(NULL, "prospective_scheduler_training_end", 1.0f);
    return 0;
}

int prospective_scheduler_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prospective_scheduler_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    prospective_scheduler_heartbeat_instance(NULL, "prospective_scheduler_training_step", progress);
    return 0;
}
