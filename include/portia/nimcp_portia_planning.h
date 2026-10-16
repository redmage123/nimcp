/**
 * @file nimcp_portia_planning.h
 * @brief Portia Spider Planning System - Memory-Constrained Route Planning
 *
 * WHAT: Planning system optimized for constrained platforms inspired by Portia spiders
 * WHY:  Portia spiders plan complex routes with limited working memory, including detours
 * HOW:  Confidence-based waypoint management with blind planning and backtracking
 *
 * BIOLOGICAL MODEL:
 * ```
 * PORTIA SPIDER BEHAVIOR              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Visual scanning                  → Plan scanning/evaluation state
 * Limited working memory           → Configurable waypoint limit
 * Detour planning (target unseen)  → Detour depth tracking
 * Confidence in target location    → Waypoint confidence decay
 * Opportunistic replanning         → Dynamic re-evaluation
 * Backtracking when blocked        → Obstacle handling & backtrack
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════╗
 * ║                    PORTIA PLANNING SYSTEM                          ║
 * ║  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ ║
 * ║  │   Scan      │→ │  Evaluate   │→ │   Execute   │→ │ Complete │ ║
 * ║  │  Environment│  │   Routes    │  │   Steps     │  │          │ ║
 * ║  └─────────────┘  └─────────────┘  └──────┬──────┘  └──────────┘ ║
 * ║                                            │                       ║
 * ║                    ┌──────────────────────┐                       ║
 * ║                    │  Obstacle Handler    │                       ║
 * ║                    │  (Detour/Backtrack)  │                       ║
 * ║                    └──────────────────────┘                       ║
 * ╚═══════════════════════════════════════════════════════════════════╝
 * ```
 *
 * KEY FEATURES:
 * - Memory-limited planning (configurable waypoint max)
 * - Confidence-based waypoint management
 * - Blind planning with detour depth tracking
 * - Opportunistic replanning when better routes found
 * - Backtracking support when plans fail
 * - Bio-async event broadcasting for coordination
 * - Thread-safe operation
 *
 * USAGE EXAMPLE:
 * ```c
 * // Initialize planner
 * portia_planning_config_t config = {
 *     .max_waypoints = 16,
 *     .max_plans = 4,
 *     .max_detour_depth = 3,
 *     .scan_interval_s = 0.1f,
 *     .confidence_threshold = 0.6f,
 *     .enable_backtracking = true
 * };
 * portia_planner_t planner = portia_planning_init(&config, bio_ctx);
 *
 * // Create plan to target
 * portia_plan_t* plan = portia_planning_create_plan(planner, target_x, target_y, target_z);
 *
 * // Add intermediate waypoint
 * portia_planning_add_waypoint(planner, plan->id, wp_x, wp_y, wp_z, 0.9f);
 *
 * // Execute planning loop
 * while (plan->state != PLAN_STATE_COMPLETE && plan->state != PLAN_STATE_FAILED) {
 *     portia_planning_evaluate(planner, plan->id);
 *     portia_planning_execute_step(planner, plan->id);
 * }
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 * - BBB security validation
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#ifndef NIMCP_PORTIA_PLANNING_H
#define NIMCP_PORTIA_PLANNING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_planner_struct* portia_planner_t;

//=============================================================================
// Planning States
//=============================================================================

/**
 * @brief States in the Portia planning lifecycle
 */
typedef enum {
    PLAN_STATE_IDLE = 0,        /**< Plan not started */
    PLAN_STATE_SCANNING,        /**< Scanning environment */
    PLAN_STATE_EVALUATING,      /**< Evaluating route options */
    PLAN_STATE_EXECUTING,       /**< Executing current plan */
    PLAN_STATE_DETOUR,          /**< Taking blind detour */
    PLAN_STATE_COMPLETE,        /**< Plan completed successfully */
    PLAN_STATE_FAILED           /**< Plan failed (exhausted options) */
} plan_state_t;

//=============================================================================
// Waypoint Structure
//=============================================================================

/**
 * @brief Waypoint in a plan with confidence tracking
 *
 * WHAT: Individual point in a route with visibility/confidence
 * WHY:  Portia spiders track confidence in unseen locations
 * HOW:  Confidence decays based on time since last seen
 */
typedef struct {
    float x, y, z;              /**< 3D position */
    float confidence;           /**< How sure we are (0.0-1.0) */
    uint64_t last_seen_ms;      /**< Timestamp when last observed */
    bool visible;               /**< Currently visible */
} plan_waypoint_t;

//=============================================================================
// Plan Structure
//=============================================================================

/**
 * @brief Complete plan with waypoints and state
 *
 * WHAT: Route plan with waypoints, state, and metrics
 * WHY:  Encapsulates all planning information
 * HOW:  Dynamic waypoint array with state tracking
 */
typedef struct {
    uint32_t id;                /**< Unique plan identifier */
    plan_waypoint_t* waypoints; /**< Array of waypoints */
    uint32_t waypoint_count;    /**< Number of waypoints */
    uint32_t current_waypoint;  /**< Currently executing waypoint */
    plan_state_t state;         /**< Current plan state */
    float total_cost;           /**< Estimated total cost/distance */
    float progress;             /**< Progress percentage (0.0-1.0) */
    bool requires_detour;       /**< Plan requires blind navigation */
    uint32_t detour_depth;      /**< How many "out of sight" segments */
} portia_plan_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for Portia planner
 *
 * WHAT: Planning parameters and limits
 * WHY:  Configure memory constraints and behavior
 * HOW:  Set max waypoints, plans, detour depth, etc.
 */
typedef struct {
    uint32_t max_waypoints;      /**< Max waypoints per plan (memory limit) */
    uint32_t max_plans;          /**< Max concurrent plans */
    uint32_t max_detour_depth;   /**< Max consecutive blind waypoints */
    float scan_interval_s;       /**< Re-evaluation interval (seconds) */
    float confidence_threshold;  /**< Min confidence to act on waypoint */
    bool enable_backtracking;    /**< Allow backtracking on failure */
} portia_planning_config_t;

//=============================================================================
// Core Planning Functions
//=============================================================================

/**
 * @brief Initialize Portia planning system
 *
 * WHAT: Create planner instance with configuration
 * WHY:  Initialize planning infrastructure
 * HOW:  Allocate structures, register bio-async handlers
 *
 * @param config Planning configuration
 * @param bio_ctx Bio-async context for event broadcasting (may be NULL)
 * @return Planner handle or NULL on failure
 */
NIMCP_EXPORT portia_planner_t portia_planning_init(
    const portia_planning_config_t* config,
    bio_module_context_t bio_ctx
);

/**
 * @brief Destroy planner and free all resources
 *
 * WHAT: Clean up planner instance
 * WHY:  Free all allocated memory
 * HOW:  Free plans, waypoints, and planner structure
 *
 * @param planner Planner to destroy
 */
NIMCP_EXPORT void portia_planning_destroy(portia_planner_t planner);

/**
 * @brief Create new plan to target position
 *
 * WHAT: Start planning route to target
 * WHY:  Initialize new plan with target waypoint
 * HOW:  Allocate plan, add target, return plan handle
 *
 * @param planner Planner instance
 * @param target_x Target X coordinate
 * @param target_y Target Y coordinate
 * @param target_z Target Z coordinate
 * @return Plan structure or NULL on failure
 */
NIMCP_EXPORT portia_plan_t* portia_planning_create_plan(
    portia_planner_t planner,
    float target_x,
    float target_y,
    float target_z
);

/**
 * @brief Add waypoint to existing plan
 *
 * WHAT: Insert intermediate waypoint in plan
 * WHY:  Build multi-step route to target
 * HOW:  Add waypoint before target, check memory limits
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @param x Waypoint X coordinate
 * @param y Waypoint Y coordinate
 * @param z Waypoint Z coordinate
 * @param confidence Initial confidence (0.0-1.0)
 * @return True on success, false on failure
 */
NIMCP_EXPORT bool portia_planning_add_waypoint(
    portia_planner_t planner,
    uint32_t plan_id,
    float x,
    float y,
    float z,
    float confidence
);

/**
 * @brief Re-evaluate current plan
 *
 * WHAT: Check plan validity and look for better routes
 * WHY:  Portia spiders opportunistically replan
 * HOW:  Update confidence, check detour feasibility
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @return True if plan still valid, false otherwise
 */
NIMCP_EXPORT bool portia_planning_evaluate(
    portia_planner_t planner,
    uint32_t plan_id
);

/**
 * @brief Execute next planning step
 *
 * WHAT: Advance to next waypoint or state
 * WHY:  Progress plan execution
 * HOW:  Move to next waypoint, update state
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @return True on success, false on failure
 */
NIMCP_EXPORT bool portia_planning_execute_step(
    portia_planner_t planner,
    uint32_t plan_id
);

/**
 * @brief Handle obstacle encountered during execution
 *
 * WHAT: Replan around obstacle
 * WHY:  Adapt to environment changes
 * HOW:  Trigger detour or backtracking
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @param obstacle_x Obstacle X coordinate
 * @param obstacle_y Obstacle Y coordinate
 * @param obstacle_z Obstacle Z coordinate
 * @return True if replanned successfully, false if plan failed
 */
NIMCP_EXPORT bool portia_planning_handle_obstacle(
    portia_planner_t planner,
    uint32_t plan_id,
    float obstacle_x,
    float obstacle_y,
    float obstacle_z
);

/**
 * @brief Check if detour planning is possible
 *
 * WHAT: Verify if blind planning can proceed
 * WHY:  Portia spiders can plan when target out of sight
 * HOW:  Check detour depth limit and confidence
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @return True if detour possible, false otherwise
 */
NIMCP_EXPORT bool portia_planning_can_detour(
    portia_planner_t planner,
    uint32_t plan_id
);

/**
 * @brief Get current plan state
 *
 * WHAT: Query plan execution state
 * WHY:  Check progress and completion
 * HOW:  Return plan state enum
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @return Current plan state
 */
NIMCP_EXPORT plan_state_t portia_planning_get_state(
    portia_planner_t planner,
    uint32_t plan_id
);

/**
 * @brief Get plan by ID
 *
 * WHAT: Retrieve plan structure by ID
 * WHY:  Access plan details for inspection
 * HOW:  Look up plan in planner's plan array
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @return Plan structure or NULL if not found
 */
NIMCP_EXPORT portia_plan_t* portia_planning_get_plan(
    portia_planner_t planner,
    uint32_t plan_id
);

/**
 * @brief Delete plan and free resources
 *
 * WHAT: Remove plan from planner
 * WHY:  Free completed or failed plans
 * HOW:  Free waypoints and remove from plan array
 *
 * @param planner Planner instance
 * @param plan_id Plan identifier
 * @return True on success, false on failure
 */
NIMCP_EXPORT bool portia_planning_delete_plan(
    portia_planner_t planner,
    uint32_t plan_id
);

/**
 * @brief Get last error message
 *
 * WHAT: Retrieve thread-local error string
 * WHY:  Debugging and error reporting
 * HOW:  Return static thread-local buffer
 *
 * @return Error message string
 */
NIMCP_EXPORT const char* portia_planning_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_PLANNING_H */
