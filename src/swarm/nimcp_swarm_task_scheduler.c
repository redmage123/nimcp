#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_swarm_task_scheduler.c - Capability-Aware Task Scheduler
//=============================================================================
/**
 * @file nimcp_swarm_task_scheduler.c
 * @brief Implementation of intelligent task-to-agent matching
 *
 * WHAT: Multi-algorithm task scheduler with capability matching
 * WHY:  Optimal work distribution across heterogeneous agents
 * HOW:  Score-based agent selection with configurable weights
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "swarm/nimcp_swarm_task_scheduler.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_task_scheduler)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Registered agent entry
 */
typedef struct {
    swarm_agent_profile_t profile;       /**< Agent capability profile */
    swarm_task_queue_t* queue;           /**< Agent's task queue */
    bool registered;                     /**< Is this slot in use? */
} agent_entry_t;

/**
 * @brief Internal scheduler structure
 */
struct swarm_task_scheduler {
    /** Configuration */
    swarm_scheduler_config_t config;

    /** Associated task manager */
    swarm_task_manager_t* task_manager;

    /** Registered agents */
    agent_entry_t agents[SWARM_SCHEDULER_MAX_AGENTS];
    uint32_t agent_count;
    uint32_t available_count;

    /** Round-robin state */
    uint32_t rr_next_agent;

    /** Statistics */
    swarm_scheduler_stats_t stats;

    /** Thread safety */
    nimcp_mutex_t* mutex;

    /** Last reschedule time */
    uint64_t last_reschedule_ms;
};

//=============================================================================
// Static Name Arrays
//=============================================================================

static const char* SCHEDULER_ALGORITHM_NAMES[] = {
    "ROUND_ROBIN",
    "CAPABILITY_MATCH",
    "LOAD_BALANCE",
    "ENERGY_AWARE",
    "LOCALITY_AWARE",
    "DEADLINE_DRIVEN",
    "HYBRID"
};

static const char* SCHEDULE_RESULT_NAMES[] = {
    "SUCCESS",
    "NO_CAPABLE_AGENT",
    "ALL_AGENTS_BUSY",
    "DEPENDENCIES_UNMET",
    "DEADLINE_INFEASIBLE",
    "INVALID_TASK",
    "ERROR"
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Check if agent meets task requirements
 */
static bool agent_meets_requirements_internal(
    const swarm_agent_profile_t* profile,
    const swarm_task_requirements_t* req)
{
    // Check availability
    if (!profile->is_available) {
        return false;
    }

    // Check required capabilities
    if ((profile->capabilities_mask & req->required_capabilities) !=
        req->required_capabilities) {
        return false;
    }

    // Check minimum proficiency for each required capability
    for (uint32_t i = 0; i < req->proficiency_count; i++) {
        // Find which capability this proficiency requirement is for
        uint32_t cap_bit = 1U << i;
        if (req->required_capabilities & cap_bit) {
            if (i < NIMCP_SWARM_CAP_COUNT &&
                profile->proficiency[i] < req->min_proficiency[i]) {
                return false;
            }
        }
    }

    // Check energy level
    if (profile->energy_level < req->min_energy) {
        return false;
    }

    return true;
}

/**
 * @brief Compute capability match score
 */
static float compute_capability_score_internal(
    const swarm_agent_profile_t* profile,
    const swarm_task_requirements_t* req)
{
    if (!agent_meets_requirements_internal(profile, req)) {
        return 0.0f;
    }

    float score = 0.0f;
    uint32_t cap_count = 0;

    // Average proficiency for required capabilities
    for (uint32_t i = 0; i < NIMCP_SWARM_CAP_COUNT; i++) {
        uint32_t cap_bit = 1U << i;
        if (req->required_capabilities & cap_bit) {
            score += profile->proficiency[i];
            cap_count++;
        }
    }

    if (cap_count > 0) {
        score /= (float)cap_count;
    }

    return score;
}

/**
 * @brief Compute load balance score (inverse of current load)
 */
static float compute_load_score(const agent_entry_t* agent)
{
    if (!agent->queue) {
        return 1.0f;  // No queue = no load
    }

    float load = swarm_task_queue_get_load(agent->queue);
    return 1.0f - load;  // Inverse: lower load = higher score
}

/**
 * @brief Compute energy score
 */
static float compute_energy_score(const swarm_agent_profile_t* profile)
{
    return profile->energy_level;
}

/**
 * @brief Compute locality score (distance-based)
 */
static float compute_locality_score(
    const swarm_agent_profile_t* profile,
    const swarm_task_requirements_t* req)
{
    if (!req->location_specified) {
        return 1.0f;  // No location preference
    }

    // Euclidean distance
    float dx = profile->position.x - req->preferred_location.x;
    float dy = profile->position.y - req->preferred_location.y;
    float dz = profile->position.z - req->preferred_location.z;
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);

    // Convert to score: closer = higher score
    if (req->max_distance > 0.0f) {
        float normalized = distance / req->max_distance;
        if (normalized > 1.0f) normalized = 1.0f;
        return 1.0f - normalized;
    }

    // No max distance: use exponential decay
    return expf(-distance / 100.0f);  // 100 unit decay constant
}

/**
 * @brief Compute deadline feasibility score
 */
static float compute_deadline_score(
    const agent_entry_t* agent,
    const swarm_task_t* task)
{
    if (task->deadline_ms == 0) {
        return 1.0f;  // No deadline
    }

    uint64_t now = nimcp_time_get_ms();
    if (now >= task->deadline_ms) {
        return 0.0f;  // Already past deadline
    }

    uint64_t time_available = task->deadline_ms - now;
    uint64_t completion_time = swarm_task_queue_estimated_completion(agent->queue);

    if (completion_time >= task->deadline_ms) {
        return 0.0f;  // Can't meet deadline
    }

    // How much margin we have
    uint64_t margin = task->deadline_ms - completion_time;
    float margin_ratio = (float)margin / (float)time_available;

    return margin_ratio > 1.0f ? 1.0f : margin_ratio;
}

/**
 * @brief Score an agent for a task using the configured algorithm
 */
static void score_agent_for_task(
    swarm_task_scheduler_t* scheduler,
    const agent_entry_t* agent,
    const swarm_task_t* task,
    swarm_agent_score_t* score)
{
    memset(score, 0, sizeof(swarm_agent_score_t));
    score->agent_id = agent->profile.agent_id;

    // Check basic capability
    score->is_capable = agent_meets_requirements_internal(
        &agent->profile, &task->requirements);

    if (!score->is_capable) {
        return;
    }

    // Compute individual scores
    score->capability_score = compute_capability_score_internal(
        &agent->profile, &task->requirements);
    score->load_score = compute_load_score(agent);
    score->energy_score = compute_energy_score(&agent->profile);
    score->locality_score = compute_locality_score(
        &agent->profile, &task->requirements);
    score->deadline_score = compute_deadline_score(agent, task);

    // Compute total based on algorithm
    const swarm_scheduler_weights_t* w = &scheduler->config.weights;

    switch (scheduler->config.algorithm) {
        case SWARM_SCHEDULER_ROUND_ROBIN:
            score->total_score = 1.0f;  // All equal
            break;

        case SWARM_SCHEDULER_CAPABILITY_MATCH:
            score->total_score = score->capability_score;
            break;

        case SWARM_SCHEDULER_LOAD_BALANCE:
            score->total_score = score->load_score;
            break;

        case SWARM_SCHEDULER_ENERGY_AWARE:
            score->total_score = score->energy_score;
            break;

        case SWARM_SCHEDULER_LOCALITY_AWARE:
            score->total_score = score->locality_score;
            break;

        case SWARM_SCHEDULER_DEADLINE_DRIVEN:
            score->total_score = score->deadline_score;
            break;

        case SWARM_SCHEDULER_HYBRID:
        default:
            score->total_score =
                w->capability_weight * score->capability_score +
                w->load_weight * score->load_score +
                w->energy_weight * score->energy_score +
                w->locality_weight * score->locality_score +
                w->deadline_weight * score->deadline_score;
            break;
    }
}

/**
 * @brief Find next available agent for round-robin
 */
static int find_next_rr_agent(
    swarm_task_scheduler_t* scheduler,
    const swarm_task_t* task)
{
    uint32_t start = scheduler->rr_next_agent;
    uint32_t current = start;

    do {
        agent_entry_t* agent = &scheduler->agents[current];

        if (agent->registered && agent->profile.is_available) {
            if (agent_meets_requirements_internal(
                    &agent->profile, &task->requirements)) {
                // Check queue capacity
                if (!agent->queue ||
                    !swarm_task_queue_is_full(agent->queue)) {
                    scheduler->rr_next_agent = (current + 1) % SWARM_SCHEDULER_MAX_AGENTS;
                    return (int)current;
                }
            }
        }

        current = (current + 1) % SWARM_SCHEDULER_MAX_AGENTS;
    } while (current != start);

    return -1;  /* No suitable agent found - normal search miss */
}

/**
 * @brief Find best agent based on scoring
 */
static int find_best_scoring_agent(
    swarm_task_scheduler_t* scheduler,
    const swarm_task_t* task,
    float* best_score)
{
    int best_index = -1;
    float max_score = -FLT_MAX;

    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        agent_entry_t* agent = &scheduler->agents[i];

        if (!agent->registered || !agent->profile.is_available) {
            continue;
        }

        // Check queue capacity
        if (agent->queue && swarm_task_queue_is_full(agent->queue)) {
            continue;
        }

        swarm_agent_score_t score;
        score_agent_for_task(scheduler, agent, task, &score);

        if (score.is_capable && score.total_score > max_score) {
            max_score = score.total_score;
            best_index = (int)i;
        }
    }

    if (best_score) {
        *best_score = max_score;
    }

    return best_index;
}

//=============================================================================
// Scheduler API Implementation
//=============================================================================

void swarm_scheduler_default_config(swarm_scheduler_config_t* config)
{
    if (!config) return;

    config->algorithm = SWARM_SCHEDULER_HYBRID;
    swarm_scheduler_default_weights(&config->weights);
    config->max_tasks_per_agent = 32;
    config->min_energy_threshold = 0.1f;
    config->auto_schedule = true;
    config->enable_bio_async = true;
    config->enable_rescheduling = true;
    config->reschedule_interval_ms = 1000;
}

void swarm_scheduler_default_weights(swarm_scheduler_weights_t* weights)
{
    if (!weights) return;

    weights->capability_weight = 0.3f;
    weights->load_weight = 0.25f;
    weights->energy_weight = 0.15f;
    weights->locality_weight = 0.15f;
    weights->deadline_weight = 0.15f;
}

swarm_task_scheduler_t* swarm_scheduler_create(
    swarm_task_manager_t* task_manager,
    const swarm_scheduler_config_t* config)
{
    if (!task_manager) {
        NIMCP_LOGGING_ERROR("Task manager required for scheduler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Task manager required for scheduler");
        return NULL;
    }

    swarm_task_scheduler_t* scheduler = nimcp_malloc(sizeof(swarm_task_scheduler_t));
    if (!scheduler) {
        NIMCP_LOGGING_ERROR("Failed to allocate task scheduler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate task scheduler");
        return NULL;
    }

    memset(scheduler, 0, sizeof(swarm_task_scheduler_t));

    // Apply configuration
    if (config) {
        scheduler->config = *config;
    } else {
        swarm_scheduler_default_config(&scheduler->config);
    }

    scheduler->task_manager = task_manager;

    // Allocate mutex
    scheduler->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!scheduler->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate scheduler mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate scheduler mutex");
        nimcp_free(scheduler);
        return NULL;
    }
    nimcp_mutex_init(scheduler->mutex, NULL);

    scheduler->last_reschedule_ms = nimcp_time_get_ms();

    NIMCP_LOGGING_INFO("Created task scheduler (algorithm=%s)",
                       swarm_scheduler_algorithm_name(scheduler->config.algorithm));

    return scheduler;
}

void swarm_scheduler_destroy(swarm_task_scheduler_t* scheduler)
{
    if (!scheduler) return;

    nimcp_mutex_lock(scheduler->mutex);

    // Destroy agent queues
    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        if (scheduler->agents[i].queue) {
            swarm_task_queue_destroy(scheduler->agents[i].queue);
        }
    }

    nimcp_mutex_unlock(scheduler->mutex);
    nimcp_mutex_destroy(scheduler->mutex);
    nimcp_free(scheduler->mutex);

    nimcp_free(scheduler);

    NIMCP_LOGGING_INFO("Destroyed task scheduler");
}

//=============================================================================
// Agent Management Implementation
//=============================================================================

int swarm_scheduler_register_agent(
    swarm_task_scheduler_t* scheduler,
    const swarm_agent_profile_t* profile)
{
    if (!scheduler || !profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_register_agent: required parameter is NULL (scheduler, profile)");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);

    // Find free slot or existing entry
    int slot = -1;
    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        if (!scheduler->agents[i].registered) {
            if (slot < 0) slot = (int)i;
        } else if (scheduler->agents[i].profile.agent_id == profile->agent_id) {
            // Already registered, update instead
            scheduler->agents[i].profile = *profile;
            if (profile->is_available && !scheduler->agents[i].profile.is_available) {
                scheduler->available_count++;
            } else if (!profile->is_available && scheduler->agents[i].profile.is_available) {
                scheduler->available_count--;
            }
            nimcp_mutex_unlock(scheduler->mutex);
            return 0;
        }
    }

    if (slot < 0) {
        NIMCP_LOGGING_WARN("No free slots for agent %u", profile->agent_id);
        nimcp_mutex_unlock(scheduler->mutex);
        return -2;
    }

    // Register in found slot
    agent_entry_t* entry = &scheduler->agents[slot];
    entry->profile = *profile;
    entry->registered = true;

    // Create task queue for agent
    swarm_task_queue_config_t queue_cfg;
    swarm_task_queue_default_config(&queue_cfg);
    queue_cfg.capacity = scheduler->config.max_tasks_per_agent;

    entry->queue = swarm_task_queue_create(profile->agent_id, &queue_cfg);
    if (!entry->queue) {
        entry->registered = false;
        nimcp_mutex_unlock(scheduler->mutex);
        return -3;
    }

    scheduler->agent_count++;
    if (profile->is_available) {
        scheduler->available_count++;
    }

    NIMCP_LOGGING_INFO("Registered agent %u (total: %u, available: %u)",
                       profile->agent_id,
                       scheduler->agent_count,
                       scheduler->available_count);

    nimcp_mutex_unlock(scheduler->mutex);

    return 0;
}

int swarm_scheduler_unregister_agent(
    swarm_task_scheduler_t* scheduler,
    uint32_t agent_id)
{
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_unregister_agent: scheduler is NULL");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);

    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        agent_entry_t* entry = &scheduler->agents[i];

        if (entry->registered && entry->profile.agent_id == agent_id) {
            // Destroy queue
            if (entry->queue) {
                swarm_task_queue_destroy(entry->queue);
                entry->queue = NULL;
            }

            if (entry->profile.is_available) {
                scheduler->available_count--;
            }
            scheduler->agent_count--;

            entry->registered = false;

            NIMCP_LOGGING_INFO("Unregistered agent %u", agent_id);

            nimcp_mutex_unlock(scheduler->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(scheduler->mutex);
    return -2;  // Not found
}

int swarm_scheduler_update_agent(
    swarm_task_scheduler_t* scheduler,
    const swarm_agent_profile_t* profile)
{
    if (!scheduler || !profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_update_agent: required parameter is NULL (scheduler, profile)");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);

    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        agent_entry_t* entry = &scheduler->agents[i];

        if (entry->registered && entry->profile.agent_id == profile->agent_id) {
            bool was_available = entry->profile.is_available;
            entry->profile = *profile;

            // Update available count
            if (profile->is_available && !was_available) {
                scheduler->available_count++;
            } else if (!profile->is_available && was_available) {
                scheduler->available_count--;
            }

            nimcp_mutex_unlock(scheduler->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(scheduler->mutex);
    return -2;  // Not found
}

swarm_task_queue_t* swarm_scheduler_get_agent_queue(
    swarm_task_scheduler_t* scheduler,
    uint32_t agent_id)
{
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_get_agent_queue: scheduler is NULL");
        return NULL;
    }

    nimcp_mutex_lock(scheduler->mutex);

    swarm_task_queue_t* queue = NULL;
    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        if (scheduler->agents[i].registered &&
            scheduler->agents[i].profile.agent_id == agent_id) {
            queue = scheduler->agents[i].queue;
            break;
        }
    }

    nimcp_mutex_unlock(scheduler->mutex);

    return queue;
}

int swarm_scheduler_set_agent_available(
    swarm_task_scheduler_t* scheduler,
    uint32_t agent_id,
    bool is_available)
{
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_set_agent_available: scheduler is NULL");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);

    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        agent_entry_t* entry = &scheduler->agents[i];

        if (entry->registered && entry->profile.agent_id == agent_id) {
            if (is_available != entry->profile.is_available) {
                entry->profile.is_available = is_available;
                if (is_available) {
                    scheduler->available_count++;
                } else {
                    scheduler->available_count--;
                }
            }

            nimcp_mutex_unlock(scheduler->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(scheduler->mutex);
    return -2;
}

//=============================================================================
// Scheduling API Implementation
//=============================================================================

swarm_schedule_result_t swarm_scheduler_schedule_task(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t* assigned_agent)
{
    if (!scheduler) {
        return SWARM_SCHEDULE_ERROR;
    }

    uint64_t start_time = nimcp_time_get_ms();

    nimcp_mutex_lock(scheduler->mutex);

    // Get task from manager
    const swarm_task_t* task = swarm_task_get(scheduler->task_manager, task_id);
    if (!task) {
        nimcp_mutex_unlock(scheduler->mutex);
        scheduler->stats.total_failed++;
        return SWARM_SCHEDULE_INVALID_TASK;
    }

    if (task->status != SWARM_TASK_STATUS_QUEUED) {
        nimcp_mutex_unlock(scheduler->mutex);
        scheduler->stats.total_failed++;
        return SWARM_SCHEDULE_INVALID_TASK;
    }

    // Check dependencies
    if (!swarm_task_dependencies_satisfied(scheduler->task_manager, task_id)) {
        nimcp_mutex_unlock(scheduler->mutex);
        return SWARM_SCHEDULE_DEPENDENCIES_UNMET;
    }

    // Find best agent
    int agent_index;
    if (scheduler->config.algorithm == SWARM_SCHEDULER_ROUND_ROBIN) {
        agent_index = find_next_rr_agent(scheduler, task);
    } else {
        agent_index = find_best_scoring_agent(scheduler, task, NULL);
    }

    if (agent_index < 0) {
        nimcp_mutex_unlock(scheduler->mutex);

        // Determine why we failed
        bool any_capable = false;
        for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
            if (scheduler->agents[i].registered &&
                agent_meets_requirements_internal(
                    &scheduler->agents[i].profile, &task->requirements)) {
                any_capable = true;
                break;
            }
        }

        if (!any_capable) {
            scheduler->stats.capability_failures++;
            scheduler->stats.total_failed++;
            return SWARM_SCHEDULE_NO_CAPABLE_AGENT;
        } else {
            scheduler->stats.load_failures++;
            scheduler->stats.total_failed++;
            return SWARM_SCHEDULE_ALL_AGENTS_BUSY;
        }
    }

    agent_entry_t* agent = &scheduler->agents[agent_index];

    // Assign task
    int result = swarm_task_assign(scheduler->task_manager, task_id,
                                   agent->profile.agent_id);
    if (result != 0) {
        nimcp_mutex_unlock(scheduler->mutex);
        scheduler->stats.total_failed++;
        return SWARM_SCHEDULE_ERROR;
    }

    // Enqueue in agent's queue
    swarm_task_queue_enqueue(
        agent->queue,
        task_id,
        task->priority,
        task->deadline_ms,
        (float)task->estimated_duration_ms
    );

    // Update stats
    scheduler->stats.total_scheduled++;
    scheduler->stats.algorithm_uses[scheduler->config.algorithm]++;

    float elapsed = (float)(nimcp_time_get_ms() - start_time);
    float count = (float)scheduler->stats.total_scheduled;
    scheduler->stats.avg_scheduling_time_us =
        ((count - 1.0f) * scheduler->stats.avg_scheduling_time_us +
         elapsed * 1000.0f) / count;

    if (assigned_agent) {
        *assigned_agent = agent->profile.agent_id;
    }

    NIMCP_LOGGING_DEBUG("Scheduled task %llu to agent %u",
                        (unsigned long long)task_id,
                        agent->profile.agent_id);

    nimcp_mutex_unlock(scheduler->mutex);

    return SWARM_SCHEDULE_SUCCESS;
}

int swarm_scheduler_schedule_all(
    swarm_task_scheduler_t* scheduler,
    uint32_t* scheduled_count)
{
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_schedule_all: scheduler is NULL");
        return -1;
    }

    uint32_t count = 0;
    uint64_t task_ids[128];
    uint32_t pending_count;

    // Get pending tasks
    if (swarm_task_get_pending(scheduler->task_manager, task_ids, 128,
                               &pending_count) != 0) {
        return -2;
    }

    // Schedule each
    for (uint32_t i = 0; i < pending_count; i++) {
        swarm_schedule_result_t result =
            swarm_scheduler_schedule_task(scheduler, task_ids[i], NULL);

        if (result == SWARM_SCHEDULE_SUCCESS) {
            count++;
        }
    }

    if (scheduled_count) {
        *scheduled_count = count;
    }

    return 0;
}

int swarm_scheduler_score_agents(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    swarm_agent_score_t* scores,
    uint32_t* count)
{
    if (!scheduler || !scores || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_score_agents: required parameter is NULL (scheduler, scores, count)");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);

    const swarm_task_t* task = swarm_task_get(scheduler->task_manager, task_id);
    if (!task) {
        nimcp_mutex_unlock(scheduler->mutex);
        return -2;
    }

    *count = 0;

    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        if (scheduler->agents[i].registered) {
            score_agent_for_task(scheduler, &scheduler->agents[i],
                                 task, &scores[*count]);
            (*count)++;
        }
    }

    nimcp_mutex_unlock(scheduler->mutex);

    return 0;
}

swarm_schedule_result_t swarm_scheduler_find_best_agent(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t* best_agent,
    float* score)
{
    if (!scheduler || !best_agent) {
        return SWARM_SCHEDULE_ERROR;
    }

    nimcp_mutex_lock(scheduler->mutex);

    const swarm_task_t* task = swarm_task_get(scheduler->task_manager, task_id);
    if (!task) {
        nimcp_mutex_unlock(scheduler->mutex);
        return SWARM_SCHEDULE_INVALID_TASK;
    }

    float best_score;
    int agent_index = find_best_scoring_agent(scheduler, task, &best_score);

    if (agent_index < 0) {
        nimcp_mutex_unlock(scheduler->mutex);
        return SWARM_SCHEDULE_NO_CAPABLE_AGENT;
    }

    *best_agent = scheduler->agents[agent_index].profile.agent_id;
    if (score) {
        *score = best_score;
    }

    nimcp_mutex_unlock(scheduler->mutex);

    return SWARM_SCHEDULE_SUCCESS;
}

int swarm_scheduler_reassign_task(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t new_agent)
{
    if (!scheduler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_reassign_task: scheduler is NULL");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);

    const swarm_task_t* task = swarm_task_get(scheduler->task_manager, task_id);
    if (!task || task->status != SWARM_TASK_STATUS_ASSIGNED) {
        nimcp_mutex_unlock(scheduler->mutex);
        return -2;
    }

    // Find old agent and remove from queue
    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        agent_entry_t* entry = &scheduler->agents[i];
        if (entry->registered &&
            entry->profile.agent_id == task->assigned_agent_id) {
            swarm_task_queue_remove(entry->queue, task_id);
            break;
        }
    }

    // Find new agent
    agent_entry_t* new_entry = NULL;
    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS; i++) {
        if (scheduler->agents[i].registered &&
            scheduler->agents[i].profile.agent_id == new_agent) {
            new_entry = &scheduler->agents[i];
            break;
        }
    }

    if (!new_entry) {
        nimcp_mutex_unlock(scheduler->mutex);
        return -3;
    }

    // Assign to new agent
    int result = swarm_task_assign(scheduler->task_manager, task_id, new_agent);
    if (result != 0) {
        nimcp_mutex_unlock(scheduler->mutex);
        return -4;
    }

    // Enqueue in new agent's queue
    swarm_task_queue_enqueue(
        new_entry->queue,
        task_id,
        task->priority,
        task->deadline_ms,
        (float)task->estimated_duration_ms
    );

    nimcp_mutex_unlock(scheduler->mutex);

    return 0;
}

uint32_t swarm_scheduler_process(swarm_task_scheduler_t* scheduler)
{
    if (!scheduler || !scheduler->config.enable_rescheduling) {
        return 0;
    }

    uint64_t now = nimcp_time_get_ms();
    if (now - scheduler->last_reschedule_ms < scheduler->config.reschedule_interval_ms) {
        return 0;
    }

    scheduler->last_reschedule_ms = now;

    // Try to schedule any pending tasks
    uint32_t scheduled;
    swarm_scheduler_schedule_all(scheduler, &scheduled);

    return scheduled;
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

int swarm_scheduler_set_algorithm(
    swarm_task_scheduler_t* scheduler,
    swarm_scheduler_algorithm_t algorithm)
{
    if (!scheduler || algorithm >= SWARM_SCHEDULER_ALGORITHM_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "swarm_scheduler_set_algorithm: scheduler is NULL");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);
    scheduler->config.algorithm = algorithm;
    nimcp_mutex_unlock(scheduler->mutex);

    return 0;
}

swarm_scheduler_algorithm_t swarm_scheduler_get_algorithm(
    const swarm_task_scheduler_t* scheduler)
{
    if (!scheduler) {
        return SWARM_SCHEDULER_ROUND_ROBIN;
    }

    return scheduler->config.algorithm;
}

int swarm_scheduler_set_weights(
    swarm_task_scheduler_t* scheduler,
    const swarm_scheduler_weights_t* weights)
{
    if (!scheduler || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_set_weights: required parameter is NULL (scheduler, weights)");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);
    scheduler->config.weights = *weights;
    nimcp_mutex_unlock(scheduler->mutex);

    return 0;
}

int swarm_scheduler_get_stats(
    const swarm_task_scheduler_t* scheduler,
    swarm_scheduler_stats_t* stats)
{
    if (!scheduler || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_get_stats: required parameter is NULL (scheduler, stats)");
        return -1;
    }

    nimcp_mutex_lock(scheduler->mutex);
    *stats = scheduler->stats;
    nimcp_mutex_unlock(scheduler->mutex);

    return 0;
}

void swarm_scheduler_reset_stats(swarm_task_scheduler_t* scheduler)
{
    if (!scheduler) return;

    nimcp_mutex_lock(scheduler->mutex);
    memset(&scheduler->stats, 0, sizeof(swarm_scheduler_stats_t));
    nimcp_mutex_unlock(scheduler->mutex);
}

//=============================================================================
// Query API Implementation
//=============================================================================

uint32_t swarm_scheduler_agent_count(const swarm_task_scheduler_t* scheduler)
{
    if (!scheduler) return 0;
    return scheduler->agent_count;
}

uint32_t swarm_scheduler_available_agent_count(
    const swarm_task_scheduler_t* scheduler)
{
    if (!scheduler) return 0;
    return scheduler->available_count;
}

uint32_t swarm_scheduler_pending_count(const swarm_task_scheduler_t* scheduler)
{
    if (!scheduler) return 0;

    uint64_t task_ids[128];
    uint32_t count;

    if (swarm_task_get_pending(scheduler->task_manager, task_ids, 128,
                               &count) != 0) {
        return 0;
    }

    return count;
}

int swarm_scheduler_get_capable_agents(
    const swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t* agent_ids,
    uint32_t max_agents,
    uint32_t* count)
{
    if (!scheduler || !agent_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_get_capable_agents: required parameter is NULL (scheduler, agent_ids, count)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)scheduler->mutex);

    const swarm_task_t* task = swarm_task_get(scheduler->task_manager, task_id);
    if (!task) {
        nimcp_mutex_unlock((nimcp_mutex_t*)scheduler->mutex);
        return -2;
    }

    *count = 0;

    for (uint32_t i = 0; i < SWARM_SCHEDULER_MAX_AGENTS && *count < max_agents; i++) {
        const agent_entry_t* entry = &scheduler->agents[i];

        if (entry->registered &&
            agent_meets_requirements_internal(&entry->profile, &task->requirements)) {
            agent_ids[(*count)++] = entry->profile.agent_id;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)scheduler->mutex);

    return 0;
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* swarm_scheduler_algorithm_name(swarm_scheduler_algorithm_t algorithm)
{
    if (algorithm >= SWARM_SCHEDULER_ALGORITHM_COUNT) {
        return "UNKNOWN";
    }
    return SCHEDULER_ALGORITHM_NAMES[algorithm];
}

const char* swarm_schedule_result_name(swarm_schedule_result_t result)
{
    if (result > SWARM_SCHEDULE_ERROR) {
        return "UNKNOWN";
    }
    return SCHEDULE_RESULT_NAMES[result];
}

bool swarm_scheduler_agent_meets_requirements(
    const swarm_agent_profile_t* profile,
    const swarm_task_requirements_t* requirements)
{
    if (!profile || !requirements) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_scheduler_agent_meets_requirements: required parameter is NULL (profile, requirements)");
        return false;
    }

    return agent_meets_requirements_internal(profile, requirements);
}

float swarm_scheduler_compute_capability_score(
    const swarm_agent_profile_t* profile,
    const swarm_task_requirements_t* requirements)
{
    if (!profile || !requirements) {
        return 0.0f;
    }

    return compute_capability_score_internal(profile, requirements);
}
