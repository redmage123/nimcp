/**
 * @file nimcp_reasoning_executive.h
 * @brief MODULE 4: Reasoning-Executive Integration
 * @version 1.0.0
 * @date 2025-11-20
 *
 * SOLE RESPONSIBILITY: Use executive functions for multi-step proof planning
 *
 * WHAT: Integration module connecting complex proofs to executive planning
 * WHY:  Multi-step proofs require goal decomposition and task management
 * HOW:  Subscribe to inference events, create executive plans for complex proofs
 *
 * BIOLOGICAL BASIS:
 * - Dorsolateral prefrontal cortex plans multi-step actions
 * - Hierarchical goal decomposition
 * - Task switching for sub-goal management
 */

#ifndef NIMCP_REASONING_EXECUTIVE_H
#define NIMCP_REASONING_EXECUTIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct executive_controller executive_controller_t;
typedef struct reasoning_executive reasoning_executive_t;

#define REASONING_EXEC_MIN_STEPS_FOR_PLANNING 3  /**< Min steps to trigger planning */
#define REASONING_EXEC_DEFAULT_PRIORITY       0.7f

typedef struct {
    uint32_t min_proof_steps_for_planning;
    float planning_priority;
    bool enable_multi_step_planning;
    bool enable_proof_decomposition;
} reasoning_executive_config_t;

typedef struct {
    uint64_t total_events_processed;
    uint64_t plans_created;
    uint64_t plans_completed;
    uint64_t plans_failed;
    float avg_plan_steps;
    uint64_t avg_callback_time_us;
} reasoning_executive_stats_t;

typedef struct {
    char goal[128];
    uint32_t total_steps;
    uint32_t completed_steps;
    float priority;
    bool is_active;
} proof_plan_t;

// Lifecycle
reasoning_executive_t* reasoning_executive_create(event_bus_t bus, executive_controller_t* executive);
reasoning_executive_t* reasoning_executive_create_custom(event_bus_t bus, executive_controller_t* executive, const reasoning_executive_config_t* config);
void reasoning_executive_destroy(reasoning_executive_t* integration);

// Core
void reasoning_executive_callback(const brain_event_t* event, void* context);
proof_plan_t* reasoning_executive_plan_proof(reasoning_executive_t* integration, const char* goal);
bool reasoning_executive_execute_step(reasoning_executive_t* integration, proof_plan_t* plan, uint32_t step);

// Query
bool reasoning_executive_get_config(const reasoning_executive_t* integration, reasoning_executive_config_t* config);
bool reasoning_executive_set_config(reasoning_executive_t* integration, const reasoning_executive_config_t* config);
bool reasoning_executive_get_stats(const reasoning_executive_t* integration, reasoning_executive_stats_t* stats);
bool reasoning_executive_reset_stats(reasoning_executive_t* integration);

// Utility
reasoning_executive_config_t reasoning_executive_default_config(void);
bool reasoning_executive_validate_config(const reasoning_executive_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_EXECUTIVE_H
