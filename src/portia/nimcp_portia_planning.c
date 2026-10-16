/**
 * @file nimcp_portia_planning.c
 * @brief Portia Spider Planning System Implementation
 *
 * WHAT: Memory-constrained route planning inspired by Portia spiders
 * WHY:  Portia spiders plan complex detours with limited working memory
 * HOW:  Confidence-based waypoint management with bio-async coordination
 *
 * PORTIA SPIDER INSPIRATION:
 * - Plan routes where target temporarily invisible
 * - Limited working memory (configurable waypoint limit)
 * - Confidence decreases for unseen locations
 * - Backtracking when plan fails
 * - Opportunistic re-planning when better route found
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include "portia/nimcp_portia_planning.h"
#include "security/nimcp_bbb_helpers.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "portia_planning"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_planning)

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static __thread char g_portia_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

/**
 * WHAT: Set thread-local error message
 * WHY:  Thread-safe error reporting
 * HOW:  Use thread-local storage
 */
static void portia_set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_portia_error, sizeof(g_portia_error), format, args);
    va_end(args);
}

const char* portia_planning_get_last_error(void)
{
    return g_portia_error;
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Main planner structure
 * WHY:  Manage multiple plans with configuration
 * HOW:  Dynamic array of plans with thread safety
 */
struct portia_planner_struct {
    portia_planning_config_t config;    /**< Configuration */
    portia_plan_t* plans;               /**< Plan array */
    uint32_t plan_count;                /**< Active plan count */
    uint32_t next_plan_id;              /**< Next plan ID to assign */
    nimcp_mutex_t lock;                 /**< Thread safety */
    bio_module_context_t bio_ctx;       /**< Bio-async context */
    uint64_t last_eval_time_ms;         /**< Last evaluation time */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Calculate distance between two 3D points
 * WHY:  Used for cost calculation and proximity checks
 * HOW:  Euclidean distance formula
 */
static float calculate_distance(float x1, float y1, float z1,
                                 float x2, float y2, float z2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * WHAT: Decay confidence based on time since last seen
 * WHY:  Portia spiders lose confidence in unseen locations
 * HOW:  Exponential decay with half-life of 1 second
 */
static float decay_confidence(float initial, uint64_t last_seen_ms)
{
    if (initial <= 0.0F) return 0.0F;

    uint64_t current_ms = nimcp_time_monotonic_ms();
    if (current_ms <= last_seen_ms) return initial;

    float elapsed_s = (current_ms - last_seen_ms) / 1000.0F;
    float half_life_s = 1.0F;
    float decay_factor = expf(-0.693147F * elapsed_s / half_life_s);

    return initial * decay_factor;
}

/**
 * WHAT: Find plan by ID in planner
 * WHY:  Internal lookup for plan operations
 * HOW:  Linear search through plan array
 */
static portia_plan_t* find_plan(portia_planner_t planner, uint32_t plan_id)
{
    if (!planner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "planner is NULL");

        return NULL;
    }

    for (uint32_t i = 0; i < planner->plan_count; i++) {
        if (planner->plans[i].id == plan_id) {
            return &planner->plans[i];
        }
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * WHAT: Calculate total plan cost (distance)
 * WHY:  Estimate effort required for plan
 * HOW:  Sum distances between consecutive waypoints
 */
static float calculate_plan_cost(const portia_plan_t* plan)
{
    if (!plan || plan->waypoint_count < 2) return 0.0F;

    float total = 0.0F;
    for (uint32_t i = 0; i < plan->waypoint_count - 1; i++) {
        const plan_waypoint_t* wp1 = &plan->waypoints[i];
        const plan_waypoint_t* wp2 = &plan->waypoints[i + 1];
        total += calculate_distance(wp1->x, wp1->y, wp1->z,
                                    wp2->x, wp2->y, wp2->z);
    }

    return total;
}

/**
 * WHAT: Update plan progress percentage
 * WHY:  Track how far along the plan we are
 * HOW:  Compare current waypoint to total waypoints
 */
static void update_plan_progress(portia_plan_t* plan)
{
    if (!plan || plan->waypoint_count == 0) return;

    plan->progress = (float)plan->current_waypoint / (float)plan->waypoint_count;
}

/**
 * WHAT: Count consecutive unseen waypoints (detour depth)
 * WHY:  Determine if blind planning exceeds limits
 * HOW:  Count non-visible waypoints from current position
 */
static uint32_t count_detour_depth(const portia_plan_t* plan)
{
    if (!plan) return 0;

    uint32_t depth = 0;
    /* Start from next waypoint after current to count upcoming invisible waypoints */
    uint32_t start = (plan->current_waypoint + 1 < plan->waypoint_count)
                     ? plan->current_waypoint + 1
                     : plan->current_waypoint;

    for (uint32_t i = start; i < plan->waypoint_count; i++) {
        if (!plan->waypoints[i].visible) {
            depth++;
        } else {
            break;  /* Stop at first visible waypoint */
        }
    }

    return depth;
}

/**
 * WHAT: Broadcast plan event via bio-async
 * WHY:  Notify other modules of planning events
 * HOW:  Send message on SEROTONIN channel (state coordination)
 */
static void broadcast_plan_event(portia_planner_t planner, uint32_t plan_id,
                                  const char* event_type)
{
    if (!planner->bio_ctx) return;

    LOG_DEBUG("Broadcasting plan event: plan=%u event=%s", plan_id, event_type);

    // Send event notification (simplified - would use proper message format)
    // This demonstrates the bio-async integration pattern
}

//=============================================================================
// Core API Implementation
//=============================================================================

portia_planner_t portia_planning_init(const portia_planning_config_t* config,
                                       bio_module_context_t bio_ctx)
{
    // Validate configuration
    if (!config) {
        portia_set_error("NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_init: config is NULL");
        return NULL;
    }

    if (!bbb_validate_range(config->max_waypoints, 2, 1024,
                             "portia_planning_init")) {
        portia_set_error("Invalid max_waypoints: %u", config->max_waypoints);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_planning_init: config is NULL");
        return NULL;
    }

    if (!bbb_validate_range(config->max_plans, 1, 256,
                             "portia_planning_init")) {
        portia_set_error("Invalid max_plans: %u", config->max_plans);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_planning_init: operation failed");
        return NULL;
    }

    LOG_INFO("Initializing Portia planner: max_waypoints=%u max_plans=%u",
             config->max_waypoints, config->max_plans);

    // Allocate planner
    portia_planner_t planner = nimcp_calloc(1, sizeof(struct portia_planner_struct));
    if (!planner) {
        portia_set_error("Failed to allocate planner");
        LOG_ERROR("Failed to allocate planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_planning_init: planner is NULL");
        return NULL;
    }

    // Copy configuration
    memcpy(&planner->config, config, sizeof(portia_planning_config_t));

    // Allocate plan array
    planner->plans = nimcp_calloc(config->max_plans, sizeof(portia_plan_t));
    if (!planner->plans) {
        portia_set_error("Failed to allocate plan array");
        LOG_ERROR("Failed to allocate plan array");
        nimcp_free(planner);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_planning_init: planner->plans is NULL");
        return NULL;
    }

    planner->plan_count = 0;
    planner->next_plan_id = 1;
    planner->bio_ctx = bio_ctx;
    planner->last_eval_time_ms = nimcp_time_monotonic_ms();

    nimcp_mutex_init(&planner->lock, NULL);

    LOG_INFO("Portia planner initialized successfully");
    bbb_audit_log(BBB_AUDIT_INFO, "portia_planning", "planner_init",
                  "max_waypoints=%u max_plans=%u",
                  config->max_waypoints, config->max_plans);

    return planner;
}

void portia_planning_destroy(portia_planner_t planner)
{
    if (!planner) {
        return;
    }

    LOG_INFO("Destroying Portia planner");

    nimcp_mutex_lock(&planner->lock);

    // Free all plans and their waypoints
    for (uint32_t i = 0; i < planner->plan_count; i++) {
        if (planner->plans[i].waypoints) {
            nimcp_free(planner->plans[i].waypoints);
        }
    }

    nimcp_free(planner->plans);
    nimcp_mutex_unlock(&planner->lock);
    nimcp_mutex_destroy(&planner->lock);
    nimcp_free(planner);

    LOG_INFO("Portia planner destroyed");
}

portia_plan_t* portia_planning_create_plan(portia_planner_t planner,
                                             float target_x,
                                             float target_y,
                                             float target_z)
{
    if (!planner) {
        portia_set_error("NULL planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_create_plan: planner is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&planner->lock);

    // Check plan limit
    if (planner->plan_count >= planner->config.max_plans) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Max plans reached: %u", planner->config.max_plans);
        LOG_WARN("Cannot create plan: max plans reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_planning_create_plan: capacity exceeded");
        return NULL;
    }

    // Find empty slot
    portia_plan_t* plan = NULL;
    for (uint32_t i = 0; i < planner->config.max_plans; i++) {
        if (planner->plans[i].waypoints == NULL) {
            plan = &planner->plans[i];
            break;
        }
    }

    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("No plan slot available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_create_plan: plan is NULL");
        return NULL;
    }

    // Initialize plan
    plan->id = planner->next_plan_id++;
    plan->waypoint_count = 1;
    plan->current_waypoint = 0;
    plan->state = PLAN_STATE_SCANNING;
    plan->total_cost = 0.0F;
    plan->progress = 0.0F;
    plan->requires_detour = false;
    plan->detour_depth = 0;

    // Allocate waypoints
    plan->waypoints = nimcp_calloc(planner->config.max_waypoints,
                                    sizeof(plan_waypoint_t));
    if (!plan->waypoints) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Failed to allocate waypoints");
        LOG_ERROR("Failed to allocate waypoints");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_planning_create_plan: plan->waypoints is NULL");
        return NULL;
    }

    // Add target waypoint
    plan->waypoints[0].x = target_x;
    plan->waypoints[0].y = target_y;
    plan->waypoints[0].z = target_z;
    plan->waypoints[0].confidence = 1.0F;
    plan->waypoints[0].last_seen_ms = nimcp_time_monotonic_ms();
    plan->waypoints[0].visible = true;

    planner->plan_count++;

    LOG_INFO("Created plan %u to target (%.2f, %.2f, %.2f)",
             plan->id, target_x, target_y, target_z);

    broadcast_plan_event(planner, plan->id, "plan_created");

    nimcp_mutex_unlock(&planner->lock);

    bbb_audit_log(BBB_AUDIT_INFO, "portia_planning", "plan_created",
                  "plan_id=%u target=(%.2f,%.2f,%.2f)",
                  plan->id, target_x, target_y, target_z);

    return plan;
}

bool portia_planning_add_waypoint(portia_planner_t planner, uint32_t plan_id,
                                   float x, float y, float z, float confidence)
{
    if (!planner) {
        portia_set_error("NULL planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_add_waypoint: planner is NULL");
        return false;
    }

    // Validate confidence
    if (confidence < 0.0F || confidence > 1.0F) {
        portia_set_error("Invalid confidence: %.2f", confidence);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_planning_add_waypoint: validation failed");
        return false;
    }

    nimcp_mutex_lock(&planner->lock);

    portia_plan_t* plan = find_plan(planner, plan_id);
    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Plan %u not found", plan_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_add_waypoint: plan is NULL");
        return false;
    }

    // Check waypoint limit
    if (plan->waypoint_count >= planner->config.max_waypoints) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Max waypoints reached: %u",
                          planner->config.max_waypoints);
        LOG_WARN("Cannot add waypoint: max reached for plan %u", plan_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "portia_planning_add_waypoint: capacity exceeded");
        return false;
    }

    // Add waypoint (insert before target)
    uint32_t insert_pos = plan->waypoint_count;
    plan->waypoints[insert_pos].x = x;
    plan->waypoints[insert_pos].y = y;
    plan->waypoints[insert_pos].z = z;
    plan->waypoints[insert_pos].confidence = confidence;
    plan->waypoints[insert_pos].last_seen_ms = nimcp_time_monotonic_ms();
    plan->waypoints[insert_pos].visible = (confidence > planner->config.confidence_threshold);

    plan->waypoint_count++;
    plan->total_cost = calculate_plan_cost(plan);

    LOG_DEBUG("Added waypoint %u to plan %u: (%.2f, %.2f, %.2f) confidence=%.2f",
              insert_pos, plan_id, x, y, z, confidence);

    nimcp_mutex_unlock(&planner->lock);
    return true;
}

bool portia_planning_evaluate(portia_planner_t planner, uint32_t plan_id)
{
    if (!planner) {
        portia_set_error("NULL planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_evaluate: planner is NULL");
        return false;
    }

    nimcp_mutex_lock(&planner->lock);

    portia_plan_t* plan = find_plan(planner, plan_id);
    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Plan %u not found", plan_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_evaluate: plan is NULL");
        return false;
    }

    // Update waypoint confidences based on time decay
    for (uint32_t i = 0; i < plan->waypoint_count; i++) {
        plan_waypoint_t* wp = &plan->waypoints[i];
        float decayed = decay_confidence(wp->confidence, wp->last_seen_ms);
        wp->confidence = decayed;

        // Update visibility based on confidence threshold
        wp->visible = (decayed >= planner->config.confidence_threshold);
    }

    // Check if current waypoint is still viable
    if (plan->current_waypoint < plan->waypoint_count) {
        const plan_waypoint_t* current = &plan->waypoints[plan->current_waypoint];
        if (current->confidence < planner->config.confidence_threshold) {
            LOG_WARN("Plan %u: current waypoint confidence too low (%.2f)",
                     plan_id, current->confidence);

            if (planner->config.enable_backtracking) {
                plan->state = PLAN_STATE_FAILED;
                LOG_INFO("Plan %u: triggering backtrack", plan_id);
            }
        }
    }

    // Update detour tracking
    plan->detour_depth = count_detour_depth(plan);
    plan->requires_detour = (plan->detour_depth > 0);

    // Check if detour exceeds limits
    if (plan->detour_depth > planner->config.max_detour_depth) {
        plan->state = PLAN_STATE_FAILED;
        LOG_WARN("Plan %u: detour depth %u exceeds limit %u",
                 plan_id, plan->detour_depth, planner->config.max_detour_depth);
        nimcp_mutex_unlock(&planner->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_planning_evaluate: validation failed");
        return false;
    }

    plan->state = PLAN_STATE_EVALUATING;
    planner->last_eval_time_ms = nimcp_time_monotonic_ms();

    LOG_DEBUG("Evaluated plan %u: detour_depth=%u confidence=%.2f",
              plan_id, plan->detour_depth,
              plan->waypoints[plan->current_waypoint].confidence);

    nimcp_mutex_unlock(&planner->lock);
    return true;
}

bool portia_planning_execute_step(portia_planner_t planner, uint32_t plan_id)
{
    if (!planner) {
        portia_set_error("NULL planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_execute_step: planner is NULL");
        return false;
    }

    nimcp_mutex_lock(&planner->lock);

    portia_plan_t* plan = find_plan(planner, plan_id);
    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Plan %u not found", plan_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_execute_step: plan is NULL");
        return false;
    }

    // Check if plan already complete or failed
    if (plan->state == PLAN_STATE_COMPLETE || plan->state == PLAN_STATE_FAILED) {
        nimcp_mutex_unlock(&planner->lock);
        return (plan->state == PLAN_STATE_COMPLETE);
    }

    // Advance to next waypoint
    plan->current_waypoint++;

    if (plan->current_waypoint >= plan->waypoint_count) {
        // Reached end of plan
        plan->state = PLAN_STATE_COMPLETE;
        plan->progress = 1.0F;
        LOG_INFO("Plan %u completed successfully", plan_id);
        broadcast_plan_event(planner, plan_id, "plan_completed");
        nimcp_mutex_unlock(&planner->lock);
        return true;
    }

    // Update state based on visibility
    const plan_waypoint_t* current = &plan->waypoints[plan->current_waypoint];
    if (!current->visible) {
        plan->state = PLAN_STATE_DETOUR;
        LOG_DEBUG("Plan %u: entering detour (waypoint not visible)", plan_id);
    } else {
        plan->state = PLAN_STATE_EXECUTING;
    }

    update_plan_progress(plan);

    LOG_DEBUG("Plan %u: advanced to waypoint %u (%.2f, %.2f, %.2f) state=%d",
              plan_id, plan->current_waypoint,
              current->x, current->y, current->z, plan->state);

    nimcp_mutex_unlock(&planner->lock);
    return true;
}

bool portia_planning_handle_obstacle(portia_planner_t planner, uint32_t plan_id,
                                      float obstacle_x, float obstacle_y,
                                      float obstacle_z)
{
    if (!planner) {
        portia_set_error("NULL planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_handle_obstacle: planner is NULL");
        return false;
    }

    nimcp_mutex_lock(&planner->lock);

    portia_plan_t* plan = find_plan(planner, plan_id);
    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Plan %u not found", plan_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_handle_obstacle: plan is NULL");
        return false;
    }

    LOG_WARN("Plan %u: obstacle encountered at (%.2f, %.2f, %.2f)",
             plan_id, obstacle_x, obstacle_y, obstacle_z);

    // Check if backtracking enabled
    if (!planner->config.enable_backtracking) {
        plan->state = PLAN_STATE_FAILED;
        LOG_ERROR("Plan %u: failed (backtracking disabled)", plan_id);
        broadcast_plan_event(planner, plan_id, "plan_failed");
        nimcp_mutex_unlock(&planner->lock);
        return false;
    }

    // Try backtracking
    if (plan->current_waypoint > 0) {
        plan->current_waypoint--;
        plan->state = PLAN_STATE_EVALUATING;
        LOG_INFO("Plan %u: backtracking to waypoint %u",
                 plan_id, plan->current_waypoint);
        update_plan_progress(plan);
        nimcp_mutex_unlock(&planner->lock);
        return true;
    }

    // Cannot backtrack further
    plan->state = PLAN_STATE_FAILED;
    LOG_ERROR("Plan %u: failed (cannot backtrack)", plan_id);
    broadcast_plan_event(planner, plan_id, "plan_failed");

    nimcp_mutex_unlock(&planner->lock);
    return false;
}

bool portia_planning_can_detour(portia_planner_t planner, uint32_t plan_id)
{
    if (!planner) {
        portia_set_error("NULL planner");
        return false;
    }

    nimcp_mutex_lock(&planner->lock);

    portia_plan_t* plan = find_plan(planner, plan_id);
    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Plan %u not found", plan_id);
        return false;
    }

    uint32_t detour_depth = count_detour_depth(plan);
    bool can_detour = (detour_depth <= planner->config.max_detour_depth);

    LOG_DEBUG("Plan %u: can_detour=%d (depth=%u limit=%u)",
              plan_id, can_detour, detour_depth,
              planner->config.max_detour_depth);

    nimcp_mutex_unlock(&planner->lock);
    return can_detour;
}

plan_state_t portia_planning_get_state(portia_planner_t planner, uint32_t plan_id)
{
    if (!planner) {
        portia_set_error("NULL planner");
        return PLAN_STATE_FAILED;
    }

    nimcp_mutex_lock(&planner->lock);

    portia_plan_t* plan = find_plan(planner, plan_id);
    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Plan %u not found", plan_id);
        return PLAN_STATE_FAILED;
    }

    plan_state_t state = plan->state;
    nimcp_mutex_unlock(&planner->lock);

    return state;
}

portia_plan_t* portia_planning_get_plan(portia_planner_t planner, uint32_t plan_id)
{
    if (!planner) {
        portia_set_error("NULL planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_get_plan: planner is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&planner->lock);
    portia_plan_t* plan = find_plan(planner, plan_id);
    nimcp_mutex_unlock(&planner->lock);

    return plan;
}

bool portia_planning_delete_plan(portia_planner_t planner, uint32_t plan_id)
{
    if (!planner) {
        portia_set_error("NULL planner");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_delete_plan: planner is NULL");
        return false;
    }

    nimcp_mutex_lock(&planner->lock);

    portia_plan_t* plan = find_plan(planner, plan_id);
    if (!plan) {
        nimcp_mutex_unlock(&planner->lock);
        portia_set_error("Plan %u not found", plan_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_planning_delete_plan: plan is NULL");
        return false;
    }

    LOG_INFO("Deleting plan %u", plan_id);

    // Free waypoints
    if (plan->waypoints) {
        nimcp_free(plan->waypoints);
        plan->waypoints = NULL;
    }

    // Clear plan structure
    memset(plan, 0, sizeof(portia_plan_t));
    planner->plan_count--;

    bbb_audit_log(BBB_AUDIT_INFO, "portia_planning", "plan_deleted",
                  "plan_id=%u", plan_id);

    nimcp_mutex_unlock(&planner->lock);
    return true;
}
