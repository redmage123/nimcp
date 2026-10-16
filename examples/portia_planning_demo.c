/**
 * @file portia_planning_demo.c
 * @brief Demonstration of Portia Planning System
 *
 * WHAT: Interactive demo of memory-constrained route planning
 * WHY:  Showcase Portia spider-inspired planning capabilities
 * HOW:  Simulate complex navigation scenarios with detours and obstacles
 *
 * SCENARIOS DEMONSTRATED:
 * 1. Simple direct path planning
 * 2. Multi-waypoint route with confidence decay
 * 3. Detour planning with limited visibility
 * 4. Obstacle handling with backtracking
 * 5. Memory-constrained planning (waypoint limits)
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portia/nimcp_portia_planning.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Print plan state name
 * WHY:  Human-readable state output
 */
static const char* state_to_string(plan_state_t state)
{
    switch (state) {
        case PLAN_STATE_IDLE:       return "IDLE";
        case PLAN_STATE_SCANNING:   return "SCANNING";
        case PLAN_STATE_EVALUATING: return "EVALUATING";
        case PLAN_STATE_EXECUTING:  return "EXECUTING";
        case PLAN_STATE_DETOUR:     return "DETOUR";
        case PLAN_STATE_COMPLETE:   return "COMPLETE";
        case PLAN_STATE_FAILED:     return "FAILED";
        default:                    return "UNKNOWN";
    }
}

/**
 * WHAT: Print waypoint details
 * WHY:  Visualize plan route
 */
static void print_waypoint(const plan_waypoint_t* wp, uint32_t index)
{
    printf("  [%u] (%.1f, %.1f, %.1f) confidence=%.2f visible=%s\n",
           index, wp->x, wp->y, wp->z, wp->confidence,
           wp->visible ? "YES" : "NO");
}

/**
 * WHAT: Print complete plan details
 * WHY:  Show full plan state
 */
static void print_plan(const portia_plan_t* plan)
{
    printf("\n=== Plan %u ===\n", plan->id);
    printf("State: %s\n", state_to_string(plan->state));
    printf("Current waypoint: %u/%u\n", plan->current_waypoint, plan->waypoint_count);
    printf("Progress: %.1f%%\n", plan->progress * 100.0f);
    printf("Total cost: %.2f\n", plan->total_cost);
    printf("Requires detour: %s (depth=%u)\n",
           plan->requires_detour ? "YES" : "NO", plan->detour_depth);
    printf("Waypoints:\n");

    for (uint32_t i = 0; i < plan->waypoint_count; i++) {
        print_waypoint(&plan->waypoints[i], i);
    }
}

/**
 * WHAT: Print scenario header
 * WHY:  Organize demo output
 */
static void print_scenario(const char* title)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════════\n");
}

//=============================================================================
// Scenario Demonstrations
//=============================================================================

/**
 * SCENARIO 1: Simple Direct Path
 */
static void demo_simple_path(portia_planner_t planner)
{
    print_scenario("SCENARIO 1: Simple Direct Path");

    printf("\nCreating plan from (0,0,0) to (10,10,0)...\n");
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);

    if (!plan) {
        printf("ERROR: Failed to create plan: %s\n",
               portia_planning_get_last_error());
        return;
    }

    print_plan(plan);

    printf("\nExecuting plan...\n");
    bool success = portia_planning_execute_step(planner, plan->id);

    if (success) {
        printf("✓ Plan completed successfully!\n");
        print_plan(plan);
    } else {
        printf("✗ Plan execution failed\n");
    }

    portia_planning_delete_plan(planner, plan->id);
}

/**
 * SCENARIO 2: Multi-Waypoint Route
 */
static void demo_multi_waypoint(portia_planner_t planner)
{
    print_scenario("SCENARIO 2: Multi-Waypoint Route");

    printf("\nCreating plan with 3 intermediate waypoints...\n");
    portia_plan_t* plan = portia_planning_create_plan(planner, 20.0f, 20.0f, 0.0f);

    if (!plan) {
        printf("ERROR: Failed to create plan\n");
        return;
    }

    // Add waypoints
    portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.9f);
    portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.85f);
    portia_planning_add_waypoint(planner, plan->id, 15.0f, 15.0f, 0.0f, 0.8f);

    printf("\nInitial plan:\n");
    print_plan(plan);

    printf("\nExecuting plan step-by-step...\n");
    for (uint32_t step = 0; step < 4; step++) {
        printf("\n--- Step %u ---\n", step + 1);

        // Evaluate before executing
        portia_planning_evaluate(planner, plan->id);

        if (plan->state == PLAN_STATE_COMPLETE || plan->state == PLAN_STATE_FAILED) {
            break;
        }

        bool success = portia_planning_execute_step(planner, plan->id);
        printf("State: %s, Current waypoint: %u, Progress: %.1f%%\n",
               state_to_string(plan->state), plan->current_waypoint,
               plan->progress * 100.0f);

        if (!success || plan->state == PLAN_STATE_COMPLETE) {
            break;
        }

        nimcp_time_sleep_ms(50);  // Simulate time passing
    }

    printf("\n✓ Final plan state:\n");
    print_plan(plan);

    portia_planning_delete_plan(planner, plan->id);
}

/**
 * SCENARIO 3: Detour with Limited Visibility
 */
static void demo_detour_planning(portia_planner_t planner)
{
    print_scenario("SCENARIO 3: Detour with Limited Visibility");

    printf("\nCreating plan with invisible waypoints (requires detour)...\n");
    portia_plan_t* plan = portia_planning_create_plan(planner, 15.0f, 15.0f, 0.0f);

    if (!plan) {
        printf("ERROR: Failed to create plan\n");
        return;
    }

    // Add waypoints with low confidence (invisible)
    portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.3f);   // Invisible
    portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.4f); // Invisible
    portia_planning_add_waypoint(planner, plan->id, 12.0f, 12.0f, 0.0f, 0.9f); // Visible

    printf("\nInitial plan (with invisible waypoints):\n");
    print_plan(plan);

    // Evaluate to check detour feasibility
    printf("\nEvaluating detour feasibility...\n");
    portia_planning_evaluate(planner, plan->id);

    bool can_detour = portia_planning_can_detour(planner, plan->id);
    printf("Can perform detour: %s\n", can_detour ? "YES" : "NO");
    printf("Detour depth: %u (limit: %u)\n",
           plan->detour_depth, 3);

    if (can_detour) {
        printf("\n✓ Detour is feasible - proceeding with blind navigation\n");

        // Execute through detour
        for (uint32_t i = 0; i < 4; i++) {
            if (plan->state == PLAN_STATE_COMPLETE || plan->state == PLAN_STATE_FAILED) {
                break;
            }

            portia_planning_execute_step(planner, plan->id);
            printf("Step %u: State=%s, Waypoint=%u\n",
                   i + 1, state_to_string(plan->state), plan->current_waypoint);
        }
    } else {
        printf("\n✗ Detour not feasible - too many invisible waypoints\n");
    }

    print_plan(plan);
    portia_planning_delete_plan(planner, plan->id);
}

/**
 * SCENARIO 4: Obstacle Handling with Backtracking
 */
static void demo_obstacle_handling(portia_planner_t planner)
{
    print_scenario("SCENARIO 4: Obstacle Handling with Backtracking");

    printf("\nCreating plan and encountering obstacle...\n");
    portia_plan_t* plan = portia_planning_create_plan(planner, 20.0f, 20.0f, 0.0f);

    if (!plan) {
        printf("ERROR: Failed to create plan\n");
        return;
    }

    portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.9f);
    portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.9f);
    portia_planning_add_waypoint(planner, plan->id, 15.0f, 15.0f, 0.0f, 0.9f);

    printf("\nInitial plan:\n");
    print_plan(plan);

    // Execute a couple steps
    printf("\nExecuting plan...\n");
    portia_planning_execute_step(planner, plan->id);
    printf("After step 1: waypoint=%u\n", plan->current_waypoint);

    portia_planning_execute_step(planner, plan->id);
    printf("After step 2: waypoint=%u\n", plan->current_waypoint);

    // Encounter obstacle
    printf("\n⚠ OBSTACLE encountered at (12, 12, 0)!\n");
    bool handled = portia_planning_handle_obstacle(planner, plan->id,
                                                    12.0f, 12.0f, 0.0f);

    if (handled) {
        printf("✓ Obstacle handled - backtracked to waypoint %u\n",
               plan->current_waypoint);
        printf("State: %s\n", state_to_string(plan->state));
    } else {
        printf("✗ Could not handle obstacle - plan failed\n");
    }

    print_plan(plan);
    portia_planning_delete_plan(planner, plan->id);
}

/**
 * SCENARIO 5: Memory-Constrained Planning
 */
static void demo_memory_constraints(portia_planner_t planner)
{
    print_scenario("SCENARIO 5: Memory-Constrained Planning");

    printf("\nAttempting to create plan exceeding waypoint limit...\n");
    portia_plan_t* plan = portia_planning_create_plan(planner, 50.0f, 50.0f, 0.0f);

    if (!plan) {
        printf("ERROR: Failed to create plan\n");
        return;
    }

    printf("Max waypoints allowed: %u\n", 16);

    // Try to add many waypoints
    uint32_t added_count = 0;
    for (uint32_t i = 1; i < 20; i++) {
        bool added = portia_planning_add_waypoint(planner, plan->id,
            (float)i * 2.5f, (float)i * 2.5f, 0.0f, 0.9f);

        if (added) {
            added_count++;
        } else {
            printf("\n⚠ Hit waypoint limit after adding %u waypoints\n", added_count);
            printf("Error: %s\n", portia_planning_get_last_error());
            break;
        }
    }

    printf("\nFinal plan (constrained by memory):\n");
    print_plan(plan);

    printf("\n✓ Memory constraint enforced successfully\n");
    printf("  This simulates Portia spider's limited working memory\n");

    portia_planning_delete_plan(planner, plan->id);
}

//=============================================================================
// Main Demo
//=============================================================================

int main(int argc, char** argv)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         PORTIA SPIDER PLANNING SYSTEM DEMONSTRATION           ║\n");
    printf("║                                                               ║\n");
    printf("║  Inspired by Portia spiders' ability to plan complex routes  ║\n");
    printf("║  with limited working memory, including blind detours        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    // Initialize logging
    nimcp_logging_config_t log_config = {
        .level = LOG_LEVEL_INFO,
        .enable_async = false,
        .enable_console = true,
        .enable_file = false
    };
    nimcp_logging_init(&log_config);

    // Initialize planner
    portia_planning_config_t config = {
        .max_waypoints = 16,
        .max_plans = 4,
        .max_detour_depth = 3,
        .scan_interval_s = 0.1f,
        .confidence_threshold = 0.6f,
        .enable_backtracking = true
    };

    printf("\nInitializing Portia planner with configuration:\n");
    printf("  - Max waypoints per plan: %u\n", config.max_waypoints);
    printf("  - Max concurrent plans: %u\n", config.max_plans);
    printf("  - Max detour depth: %u\n", config.max_detour_depth);
    printf("  - Confidence threshold: %.2f\n", config.confidence_threshold);
    printf("  - Backtracking enabled: %s\n", config.enable_backtracking ? "YES" : "NO");

    portia_planner_t planner = portia_planning_init(&config, NULL);
    if (!planner) {
        fprintf(stderr, "ERROR: Failed to initialize planner\n");
        return EXIT_FAILURE;
    }

    printf("\n✓ Planner initialized successfully\n");

    // Run demonstrations
    demo_simple_path(planner);
    demo_multi_waypoint(planner);
    demo_detour_planning(planner);
    demo_obstacle_handling(planner);
    demo_memory_constraints(planner);

    // Cleanup
    printf("\n");
    print_scenario("CLEANUP");
    printf("\nDestroying planner...\n");
    portia_planning_destroy(planner);
    printf("✓ Cleanup complete\n");

    nimcp_logging_cleanup();

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    DEMONSTRATION COMPLETE                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    return EXIT_SUCCESS;
}
