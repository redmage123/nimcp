/**
 * @file nimcp_swarm_conflict.c
 * @brief Multi-swarm conflict detection and resolution implementation
 *
 * @author NIMCP Development Team
 * @version 1.0
 * @date 2025
 */

#include "swarm/nimcp_swarm_conflict.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "swarm_conflict"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_conflict)

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Negotiation state for a conflict
 */
typedef struct {
    uint64_t conflict_id;
    uint32_t current_round;
    uint32_t max_rounds;
    negotiation_offer_t offers[NIMCP_MAX_NEGOTIATION_ROUNDS];
    uint32_t offer_count;
    bool converged;
    uint64_t start_time_us;
    uint64_t timeout_us;
} negotiation_state_t;

/**
 * @brief Strategy mapping
 */
typedef struct {
    conflict_type_t type;
    resolution_strategy_t strategy;
} strategy_mapping_t;

/**
 * @brief Conflict resolver internal structure
 */
struct swarm_conflict_resolver_struct {
    nimcp_multi_swarm_coordinator_t* coordinator;
    conflict_config_t config;

    /* Active conflicts */
    conflict_t* active_conflicts;
    uint32_t active_count;
    uint32_t active_capacity;

    /* Conflict history */
    conflict_t* history;
    uint32_t history_count;
    uint32_t history_capacity;

    /* Negotiation states */
    negotiation_state_t* negotiations;
    uint32_t negotiation_count;
    uint32_t negotiation_capacity;

    /* Strategy mappings */
    strategy_mapping_t strategy_map[CONFLICT_TYPE_COUNT];

    /* Statistics */
    conflict_stats_t stats;

    /* Bio-async */
    bio_module_context_t bio_context;

    /* Next conflict ID */
    uint64_t next_conflict_id;
};

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

static nimcp_result_t add_active_conflict(swarm_conflict_resolver_t resolver, const conflict_t* conflict);
static nimcp_result_t move_to_history(swarm_conflict_resolver_t resolver, conflict_t* conflict);
static negotiation_state_t* find_negotiation(swarm_conflict_resolver_t resolver, uint64_t conflict_id);
static conflict_t* find_active_conflict(swarm_conflict_resolver_t resolver, uint64_t conflict_id);
static bool territories_overlap(const nimcp_territory_bounds_t* a, const nimcp_territory_bounds_t* b);
static float calculate_territory_overlap(const nimcp_territory_bounds_t* a, const nimcp_territory_bounds_t* b);

/*=============================================================================
 * LIFECYCLE API IMPLEMENTATION
 *============================================================================*/

conflict_config_t conflict_resolver_default_config(void) {
    conflict_config_t config = {
        .max_conflicts = 100,
        .resolution_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS,
        .enable_negotiation = true,
        .enable_arbitration = true,
        .enable_auto_resolution = false,
        .convergence_threshold = 0.05F,
        .max_negotiation_rounds = 10,
        .default_strategy = RESOLUTION_STRATEGY_PRIORITY_WINS
    };
    return config;
}

swarm_conflict_resolver_t conflict_resolver_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const conflict_config_t* config) {

    /**
     * WHAT: Creates a new conflict resolver instance
     * WHY:  Enables conflict detection and resolution for swarms
     * HOW:  Allocates resolver structure with configuration
     */

    /* Guard: Validate inputs */
    if (!coordinator) {
        LOG_ERROR("coordinator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return NULL;
    }

    /* Allocate resolver */
    swarm_conflict_resolver_t resolver = nimcp_calloc(1, sizeof(*resolver));
    if (!resolver) {
        LOG_ERROR("Failed to allocate conflict resolver");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "resolver is NULL");

        return NULL;
    }

    /* Initialize fields */
    resolver->coordinator = coordinator;
    resolver->config = config ? *config : conflict_resolver_default_config();
    resolver->next_conflict_id = 1;

    /* Allocate active conflicts array */
    resolver->active_capacity = resolver->config.max_conflicts;
    resolver->active_conflicts = nimcp_calloc(resolver->active_capacity, sizeof(conflict_t));
    if (!resolver->active_conflicts) {
        LOG_ERROR("Failed to allocate active conflicts array");
        nimcp_free(resolver);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "conflict_resolver_default_config: resolver->active_conflicts is NULL");
        return NULL;
    }

    /* Allocate history array */
    resolver->history_capacity = NIMCP_MAX_CONFLICT_HISTORY;
    resolver->history = nimcp_calloc(resolver->history_capacity, sizeof(conflict_t));
    if (!resolver->history) {
        LOG_ERROR("Failed to allocate history array");
        nimcp_free(resolver->active_conflicts);
        nimcp_free(resolver);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "conflict_resolver_default_config: resolver->history is NULL");
        return NULL;
    }

    /* Allocate negotiations array */
    resolver->negotiation_capacity = resolver->config.max_conflicts;
    resolver->negotiations = nimcp_calloc(resolver->negotiation_capacity, sizeof(negotiation_state_t));
    if (!resolver->negotiations) {
        LOG_ERROR("Failed to allocate negotiations array");
        nimcp_free(resolver->history);
        nimcp_free(resolver->active_conflicts);
        nimcp_free(resolver);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "conflict_resolver_default_config: resolver->negotiations is NULL");
        return NULL;
    }

    /* Initialize strategy mappings with defaults */
    for (int i = 0; i < CONFLICT_TYPE_COUNT; i++) {
        resolver->strategy_map[i].type = i;
        resolver->strategy_map[i].strategy = resolver->config.default_strategy;
    }

    /* Set specific strategy defaults */
    resolver->strategy_map[CONFLICT_TYPE_RESOURCE].strategy = RESOLUTION_STRATEGY_FAIR_SHARE;
    resolver->strategy_map[CONFLICT_TYPE_TERRITORY].strategy = RESOLUTION_STRATEGY_NEGOTIATION;
    resolver->strategy_map[CONFLICT_TYPE_PRIORITY].strategy = RESOLUTION_STRATEGY_PRIORITY_WINS;

    /* Initialize statistics */
    memset(&resolver->stats, 0, sizeof(resolver->stats));
    resolver->stats.min_resolution_time_ms = INFINITY;

    LOG_INFO("Created conflict resolver with max_conflicts=%u", resolver->config.max_conflicts);

    return resolver;
}

void conflict_resolver_destroy(swarm_conflict_resolver_t resolver) {
    /**
     * WHAT: Destroys conflict resolver and frees resources
     * WHY:  Clean up resolver on shutdown
     * HOW:  Frees all tracked conflicts and internal structures
     */

    /* Guard: Validate input */
    if (!resolver) {
        return;
    }

    LOG_INFO("Destroying conflict resolver (active=%u, history=%u)",
             resolver->active_count, resolver->history_count);

    /* Free context data in active conflicts */
    for (uint32_t i = 0; i < resolver->active_count; i++) {
        if (resolver->active_conflicts[i].context_data) {
            nimcp_free(resolver->active_conflicts[i].context_data);
        }
    }

    /* Free context data in history */
    for (uint32_t i = 0; i < resolver->history_count; i++) {
        if (resolver->history[i].context_data) {
            nimcp_free(resolver->history[i].context_data);
        }
    }

    /* Free arrays */
    nimcp_free(resolver->negotiations);
    nimcp_free(resolver->history);
    nimcp_free(resolver->active_conflicts);
    nimcp_free(resolver);
}

/*=============================================================================
 * FEATURE 1: CONFLICT DETECTION IMPLEMENTATION
 *============================================================================*/

nimcp_result_t conflict_resolver_detect(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count) {

    /**
     * WHAT: Scans swarm states for resource, goal, and territory conflicts
     * WHY:  Proactive detection enables early resolution
     * HOW:  Pairwise comparison of territories, resources, and goals
     */

    /* Guard: Validate inputs */
    if (!resolver || !swarm_states || !conflicts || !count) {
        LOG_ERROR("Invalid parameters");
        return NIMCP_INVALID_PARAM;
    }

    if (state_count < 2) {
        *count = 0;
        return NIMCP_SUCCESS;
    }

    *count = 0;

    LOG_DEBUG("Detecting conflicts among %u swarms", state_count);

    /* Detect resource conflicts */
    uint32_t resource_conflicts = 0;
    nimcp_result_t result = conflict_resolver_detect_resource_conflicts(
        resolver, swarm_states, state_count,
        conflicts + *count, max_conflicts - *count, &resource_conflicts);

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to detect resource conflicts: %d", result);
        return result;
    }

    *count += resource_conflicts;
    LOG_DEBUG("Detected %u resource conflicts", resource_conflicts);

    /* Detect territory conflicts */
    if (*count < max_conflicts) {
        uint32_t territory_conflicts = 0;
        result = conflict_resolver_detect_territory_conflicts(
            resolver, swarm_states, state_count,
            conflicts + *count, max_conflicts - *count, &territory_conflicts);

        if (result != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to detect territory conflicts: %d", result);
            return result;
        }

        *count += territory_conflicts;
        LOG_DEBUG("Detected %u territory conflicts", territory_conflicts);
    }

    /* Detect goal conflicts */
    if (*count < max_conflicts) {
        uint32_t goal_conflicts = 0;
        result = conflict_resolver_detect_goal_conflicts(
            resolver, swarm_states, state_count,
            conflicts + *count, max_conflicts - *count, &goal_conflicts);

        if (result != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to detect goal conflicts: %d", result);
            return result;
        }

        *count += goal_conflicts;
        LOG_DEBUG("Detected %u goal conflicts", goal_conflicts);
    }

    /* Add conflicts to active list and update statistics */
    for (uint32_t i = 0; i < *count; i++) {
        conflicts[i].conflict_id = resolver->next_conflict_id++;
        conflicts[i].timestamp_us = nimcp_time_get_us();
        conflicts[i].is_resolved = false;

        /* Calculate severity */
        conflicts[i].severity = conflict_resolver_calculate_severity(resolver, &conflicts[i]);

        /* Add to active conflicts */
        add_active_conflict(resolver, &conflicts[i]);

        /* Update statistics */
        resolver->stats.total_conflicts++;
        resolver->stats.type_counts[conflicts[i].type]++;
        resolver->stats.conflicts_pending++;
    }

    LOG_INFO("Detected %u total conflicts", *count);

    return NIMCP_SUCCESS;
}

float conflict_resolver_calculate_severity(
    swarm_conflict_resolver_t resolver,
    const conflict_t* conflict) {

    /**
     * WHAT: Computes severity score for a conflict
     * WHY:  Enables prioritization of resolution efforts
     * HOW:  Considers resource importance, swarm priorities, and impact
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict) {
        return 0.0F;
    }

    float severity = 0.0F;

    /* Base severity by type */
    switch (conflict->type) {
        case CONFLICT_TYPE_RESOURCE:
            severity = 0.6F;
            break;
        case CONFLICT_TYPE_GOAL:
            severity = 0.7F;
            break;
        case CONFLICT_TYPE_TERRITORY:
            severity = 0.5F;
            break;
        case CONFLICT_TYPE_PRIORITY:
            severity = 0.8F;
            break;
        case CONFLICT_TYPE_COMMUNICATION:
            severity = 0.4F;
            break;
        default:
            severity = 0.5F;
    }

    /* Increase severity based on number of swarms involved */
    if (conflict->swarm_count > 2) {
        severity += (conflict->swarm_count - 2) * 0.1F;
    }

    /* Clamp to [0, 1] */
    if (severity > 1.0F) severity = 1.0F;
    if (severity < 0.0F) severity = 0.0F;

    return severity;
}

nimcp_result_t conflict_resolver_detect_resource_conflicts(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count) {

    /**
     * WHAT: Detects when multiple swarms want the same resource
     * WHY:  Resource contention must be resolved for efficiency
     * HOW:  Checks for overlapping resource requests
     */

    /* Guard: Validate inputs */
    if (!resolver || !swarm_states || !conflicts || !count) {
        return NIMCP_INVALID_PARAM;
    }

    *count = 0;

    /* Pairwise comparison of resource lists */
    for (uint32_t i = 0; i < state_count && *count < max_conflicts; i++) {
        if (!swarm_states[i].is_active) continue;

        for (uint32_t j = i + 1; j < state_count && *count < max_conflicts; j++) {
            if (!swarm_states[j].is_active) continue;

            /* Check for overlapping resources */
            for (uint32_t ri = 0; ri < swarm_states[i].resource_count; ri++) {
                for (uint32_t rj = 0; rj < swarm_states[j].resource_count; rj++) {
                    if (swarm_states[i].resource_ids[ri] == swarm_states[j].resource_ids[rj]) {
                        /* Found resource conflict */
                        conflict_t* c = &conflicts[*count];
                        memset(c, 0, sizeof(*c));

                        c->type = CONFLICT_TYPE_RESOURCE;
                        c->swarm_ids[0] = swarm_states[i].swarm_id;
                        c->swarm_ids[1] = swarm_states[j].swarm_id;
                        c->swarm_count = 2;
                        c->resource_id = swarm_states[i].resource_ids[ri];

                        snprintf(c->description, sizeof(c->description),
                                "Resource %lu contested by swarms %lu and %lu",
                                c->resource_id, c->swarm_ids[0], c->swarm_ids[1]);

                        (*count)++;
                        break;
                    }
                }
            }
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_detect_territory_conflicts(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count) {

    /**
     * WHAT: Detects overlapping operational territories
     * WHY:  Territory conflicts can cause coordination issues
     * HOW:  Checks bounding box intersections
     */

    /* Guard: Validate inputs */
    if (!resolver || !swarm_states || !conflicts || !count) {
        return NIMCP_INVALID_PARAM;
    }

    *count = 0;

    /* Pairwise comparison of territories */
    for (uint32_t i = 0; i < state_count && *count < max_conflicts; i++) {
        if (!swarm_states[i].is_active) continue;

        for (uint32_t j = i + 1; j < state_count && *count < max_conflicts; j++) {
            if (!swarm_states[j].is_active) continue;

            /* Check for territory overlap */
            if (territories_overlap(&swarm_states[i].territory, &swarm_states[j].territory)) {
                /* Found territory conflict */
                conflict_t* c = &conflicts[*count];
                memset(c, 0, sizeof(*c));

                c->type = CONFLICT_TYPE_TERRITORY;
                c->swarm_ids[0] = swarm_states[i].swarm_id;
                c->swarm_ids[1] = swarm_states[j].swarm_id;
                c->swarm_count = 2;

                float overlap = calculate_territory_overlap(&swarm_states[i].territory,
                                                           &swarm_states[j].territory);

                snprintf(c->description, sizeof(c->description),
                        "Territory overlap (%.1f%%) between swarms %lu and %lu",
                        overlap * 100.0F, c->swarm_ids[0], c->swarm_ids[1]);

                (*count)++;
            }
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_detect_goal_conflicts(
    swarm_conflict_resolver_t resolver,
    const swarm_state_t* swarm_states,
    uint32_t state_count,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count) {

    /**
     * WHAT: Detects contradictory or competing goals
     * WHY:  Goal conflicts lead to inefficient behavior
     * HOW:  Checks for mutually exclusive goals
     */

    /* Guard: Validate inputs */
    if (!resolver || !swarm_states || !conflicts || !count) {
        return NIMCP_INVALID_PARAM;
    }

    *count = 0;

    /* Pairwise comparison of goal lists */
    for (uint32_t i = 0; i < state_count && *count < max_conflicts; i++) {
        if (!swarm_states[i].is_active) continue;

        for (uint32_t j = i + 1; j < state_count && *count < max_conflicts; j++) {
            if (!swarm_states[j].is_active) continue;

            /* Check for conflicting goals */
            for (uint32_t gi = 0; gi < swarm_states[i].goal_count; gi++) {
                for (uint32_t gj = 0; gj < swarm_states[j].goal_count; gj++) {
                    /* Simple check: same goal ID might indicate conflict */
                    if (swarm_states[i].goal_ids[gi] == swarm_states[j].goal_ids[gj]) {
                        /* Found goal conflict */
                        conflict_t* c = &conflicts[*count];
                        memset(c, 0, sizeof(*c));

                        c->type = CONFLICT_TYPE_GOAL;
                        c->swarm_ids[0] = swarm_states[i].swarm_id;
                        c->swarm_ids[1] = swarm_states[j].swarm_id;
                        c->swarm_count = 2;

                        snprintf(c->description, sizeof(c->description),
                                "Goal %lu conflict between swarms %lu and %lu",
                                swarm_states[i].goal_ids[gi], c->swarm_ids[0], c->swarm_ids[1]);

                        (*count)++;
                        break;
                    }
                }
            }
        }
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * FEATURE 2: RESOLUTION STRATEGIES IMPLEMENTATION
 *============================================================================*/

nimcp_result_t conflict_resolver_set_strategy(
    swarm_conflict_resolver_t resolver,
    conflict_type_t conflict_type,
    resolution_strategy_t strategy) {

    /**
     * WHAT: Configures which strategy to use for a conflict type
     * WHY:  Different conflict types benefit from different strategies
     * HOW:  Stores strategy mapping in resolver configuration
     */

    /* Guard: Validate inputs */
    if (!resolver) {
        LOG_ERROR("resolver is NULL");
        return NIMCP_INVALID_PARAM;
    }

    if (conflict_type >= CONFLICT_TYPE_COUNT) {
        LOG_ERROR("Invalid conflict type: %d", conflict_type);
        return NIMCP_INVALID_PARAM;
    }

    if (strategy >= RESOLUTION_STRATEGY_COUNT) {
        LOG_ERROR("Invalid resolution strategy: %d", strategy);
        return NIMCP_INVALID_PARAM;
    }

    resolver->strategy_map[conflict_type].strategy = strategy;

    LOG_INFO("Set strategy for %s to %s",
             conflict_type_name(conflict_type),
             resolution_strategy_name(strategy));

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_resolve(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_strategy_t strategy,
    resolution_result_t* result) {

    /**
     * WHAT: Applies resolution strategy to resolve conflict
     * WHY:  Enables flexible conflict resolution
     * HOW:  Dispatches to strategy-specific implementation
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict || !result) {
        LOG_ERROR("Invalid parameters");
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Resolving conflict %lu (type=%s) with strategy %s",
             conflict->conflict_id,
             conflict_type_name(conflict->type),
             resolution_strategy_name(strategy));

    uint64_t start_time = nimcp_time_get_us();
    nimcp_result_t res;

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->conflict_id = conflict->conflict_id;
    result->strategy_used = strategy;

    /* Dispatch to strategy implementation */
    switch (strategy) {
        case RESOLUTION_STRATEGY_PRIORITY_WINS:
            res = conflict_resolver_priority_wins(resolver, conflict, result);
            break;

        case RESOLUTION_STRATEGY_FAIR_SHARE:
            res = conflict_resolver_fair_share(resolver, conflict, result);
            break;

        case RESOLUTION_STRATEGY_NEGOTIATION:
            res = conflict_resolver_negotiate(resolver, conflict, resolver->config.max_negotiation_rounds);
            /* Negotiation is async, mark as pending */
            result->success = false;
            snprintf(result->outcome_description, sizeof(result->outcome_description),
                    "Negotiation started");
            return NIMCP_SUCCESS;

        case RESOLUTION_STRATEGY_ARBITRATION:
            res = conflict_resolver_arbitration(resolver, conflict, result);
            break;

        case RESOLUTION_STRATEGY_YIELD:
            res = conflict_resolver_yield(resolver, conflict, result);
            break;

        default:
            LOG_ERROR("Unsupported strategy: %d", strategy);
            return NIMCP_NOT_IMPLEMENTED;
    }

    if (res != NIMCP_SUCCESS) {
        LOG_ERROR("Strategy %s failed: %d", resolution_strategy_name(strategy), res);
        result->success = false;
        return res;
    }

    /* Calculate resolution time */
    uint64_t end_time = nimcp_time_get_us();
    result->resolution_time_ms = (end_time - start_time) / 1000.0F;

    /* Mark conflict as resolved */
    conflict->is_resolved = true;
    conflict->resolution_timestamp_us = end_time;

    /* Update statistics */
    resolver->stats.conflicts_resolved++;
    resolver->stats.conflicts_pending--;
    resolver->stats.strategy_usage[strategy]++;

    float resolution_time = result->resolution_time_ms;
    float n = (float)resolver->stats.conflicts_resolved;
    resolver->stats.avg_resolution_time_ms =
        (resolver->stats.avg_resolution_time_ms * (n - 1) + resolution_time) / n;

    if (resolution_time > resolver->stats.max_resolution_time_ms) {
        resolver->stats.max_resolution_time_ms = resolution_time;
    }
    if (resolution_time < resolver->stats.min_resolution_time_ms) {
        resolver->stats.min_resolution_time_ms = resolution_time;
    }

    /* Move to history */
    move_to_history(resolver, conflict);

    LOG_INFO("Conflict %lu resolved successfully in %.2f ms",
             conflict->conflict_id, resolution_time);

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_priority_wins(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result) {

    /**
     * WHAT: Higher priority swarm wins the conflict
     * WHY:  Simple, decisive resolution for clear priority cases
     * HOW:  Compares swarm priorities, awards to highest
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict || !result) {
        return NIMCP_INVALID_PARAM;
    }

    if (conflict->swarm_count < 2) {
        LOG_ERROR("Conflict has less than 2 swarms");
        return NIMCP_INVALID_PARAM;
    }

    /* Find highest priority swarm */
    uint64_t winner_id = conflict->swarm_ids[0];
    float highest_priority = 0.0F;

    for (uint32_t i = 0; i < conflict->swarm_count; i++) {
        nimcp_swarm_identity_t* swarm = nimcp_swarm_get(resolver->coordinator, conflict->swarm_ids[i]);
        if (!swarm) continue;

        /* Use health percentage as priority proxy */
        float priority = swarm->health_percentage;
        if (priority > highest_priority) {
            highest_priority = priority;
            winner_id = conflict->swarm_ids[i];
        }
    }

    /* Set result */
    result->winner_id = winner_id;
    result->success = true;

    /* Winner gets 100%, others get 0% */
    for (uint32_t i = 0; i < conflict->swarm_count; i++) {
        result->terms[i] = (conflict->swarm_ids[i] == winner_id) ? 1.0F : 0.0F;
    }
    result->term_count = conflict->swarm_count;

    snprintf(result->outcome_description, sizeof(result->outcome_description),
            "Swarm %lu wins by priority (%.2f)", winner_id, highest_priority);

    LOG_DEBUG("Priority resolution: winner=%lu priority=%.2f", winner_id, highest_priority);

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_fair_share(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result) {

    /**
     * WHAT: Splits resource proportionally among swarms
     * WHY:  Equitable distribution for shared resources
     * HOW:  Allocates proportional to priority or capacity
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict || !result) {
        return NIMCP_INVALID_PARAM;
    }

    if (conflict->swarm_count < 2) {
        LOG_ERROR("Conflict has less than 2 swarms");
        return NIMCP_INVALID_PARAM;
    }

    /* Calculate total priority */
    float total_priority = 0.0F;
    float priorities[NIMCP_MAX_CONFLICT_SWARMS] = {0};

    for (uint32_t i = 0; i < conflict->swarm_count; i++) {
        nimcp_swarm_identity_t* swarm = nimcp_swarm_get(resolver->coordinator, conflict->swarm_ids[i]);
        if (!swarm) {
            priorities[i] = 1.0F;  /* Default */
        } else {
            priorities[i] = swarm->health_percentage;
        }
        total_priority += priorities[i];
    }

    /* Guard: Prevent division by zero */
    if (total_priority < 0.001F) {
        total_priority = (float)conflict->swarm_count;
        for (uint32_t i = 0; i < conflict->swarm_count; i++) {
            priorities[i] = 1.0F;
        }
    }

    /* Allocate proportionally */
    for (uint32_t i = 0; i < conflict->swarm_count; i++) {
        result->terms[i] = priorities[i] / total_priority;
    }
    result->term_count = conflict->swarm_count;
    result->success = true;
    result->winner_id = 0;  /* No single winner */

    snprintf(result->outcome_description, sizeof(result->outcome_description),
            "Fair share: proportional allocation based on priorities");

    LOG_DEBUG("Fair share resolution: %u swarms get proportional allocation", conflict->swarm_count);

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_arbitration(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result) {

    /**
     * WHAT: External arbiter makes final decision
     * WHY:  Neutral third party for difficult conflicts
     * HOW:  Invokes coordinator's arbitration logic
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict || !result) {
        return NIMCP_INVALID_PARAM;
    }

    if (!resolver->config.enable_arbitration) {
        LOG_ERROR("Arbitration is disabled");
        return NIMCP_INVALID_STATE;
    }

    /* For now, fall back to priority-based arbitration */
    LOG_DEBUG("Arbitration invoked for conflict %lu", conflict->conflict_id);

    nimcp_result_t res = conflict_resolver_priority_wins(resolver, conflict, result);
    if (res != NIMCP_SUCCESS) {
        return res;
    }

    /* Update description */
    snprintf(result->outcome_description, sizeof(result->outcome_description),
            "Arbitration decision: Swarm %lu", result->winner_id);

    resolver->stats.arbitrations++;

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_yield(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    resolution_result_t* result) {

    /**
     * WHAT: Lower priority swarm yields to higher priority
     * WHY:  Graceful conflict avoidance
     * HOW:  Lower priority swarm backs off
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict || !result) {
        return NIMCP_INVALID_PARAM;
    }

    /* Same as priority wins, but different semantics */
    nimcp_result_t res = conflict_resolver_priority_wins(resolver, conflict, result);
    if (res != NIMCP_SUCCESS) {
        return res;
    }

    snprintf(result->outcome_description, sizeof(result->outcome_description),
            "Swarm %lu yields to higher priority swarm %lu",
            conflict->swarm_ids[1], conflict->swarm_ids[0]);

    LOG_DEBUG("Yield resolution: swarm %lu yields to %lu",
              conflict->swarm_ids[1], conflict->swarm_ids[0]);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * FEATURE 3: NEGOTIATION PROTOCOL IMPLEMENTATION
 *============================================================================*/

nimcp_result_t conflict_resolver_negotiate(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict,
    uint32_t max_rounds) {

    /**
     * WHAT: Initiates multi-round negotiation protocol
     * WHY:  Allows swarms to reach mutually acceptable solution
     * HOW:  Sets up negotiation context and sends initial offers
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict) {
        LOG_ERROR("Invalid parameters");
        return NIMCP_INVALID_PARAM;
    }

    if (!resolver->config.enable_negotiation) {
        LOG_ERROR("Negotiation is disabled");
        return NIMCP_INVALID_STATE;
    }

    /* Guard: Check capacity */
    if (resolver->negotiation_count >= resolver->negotiation_capacity) {
        LOG_ERROR("Maximum negotiations reached");
        return NIMCP_BUFFER_FULL;
    }

    /* Create negotiation state */
    negotiation_state_t* neg = &resolver->negotiations[resolver->negotiation_count++];
    memset(neg, 0, sizeof(*neg));

    neg->conflict_id = conflict->conflict_id;
    neg->current_round = 0;
    neg->max_rounds = max_rounds;
    neg->start_time_us = nimcp_time_get_us();
    neg->timeout_us = resolver->config.resolution_timeout_ms * 1000;
    neg->converged = false;

    LOG_INFO("Started negotiation for conflict %lu (max_rounds=%u)",
             conflict->conflict_id, max_rounds);

    resolver->stats.negotiations_started++;

    /* Send initial negotiation messages via bio-async */
    if (resolver->bio_context) {
        bio_message_header_t msg = {
            .type = BIO_MSG_SWARM_NEGOTIATION_STARTED,
            .source_module = BIO_MODULE_SWARM_MULTI,
            .target_module = 0,  /* Broadcast to all */
            .flags = BIO_MSG_FLAG_BROADCAST,
            .timestamp_us = nimcp_time_get_us()
        };

        /* Send message */
        bio_router_broadcast(resolver->bio_context, &msg, sizeof(msg));
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_make_offer(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    uint64_t swarm_id,
    const float* proposal,
    uint32_t proposal_size) {

    /**
     * WHAT: Swarm proposes solution to conflict
     * WHY:  Enables collaborative problem-solving
     * HOW:  Stores offer and broadcasts to other swarms
     */

    /* Guard: Validate inputs */
    if (!resolver || !proposal) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find negotiation state */
    negotiation_state_t* neg = find_negotiation(resolver, conflict_id);
    if (!neg) {
        LOG_ERROR("No active negotiation for conflict %lu", conflict_id);
        return NIMCP_NOT_FOUND;
    }

    /* Guard: Check round limit */
    if (neg->offer_count >= NIMCP_MAX_NEGOTIATION_ROUNDS) {
        LOG_ERROR("Maximum negotiation offers reached");
        return NIMCP_BUFFER_FULL;
    }

    /* Create offer */
    negotiation_offer_t* offer = &neg->offers[neg->offer_count++];
    offer->conflict_id = conflict_id;
    offer->proposer_swarm_id = swarm_id;
    offer->round = neg->current_round;
    offer->proposal_size = proposal_size;
    offer->timestamp_us = nimcp_time_get_us();

    /* Copy proposal */
    memcpy(offer->proposal, proposal, proposal_size * sizeof(float));

    /* Calculate acceptance score (simple sum check) */
    float sum = 0.0F;
    for (uint32_t i = 0; i < proposal_size; i++) {
        sum += proposal[i];
    }
    offer->acceptance_score = fabsf(1.0F - sum) < 0.1F ? 0.9F : 0.5F;

    LOG_DEBUG("Negotiation offer from swarm %lu: round=%u score=%.2f",
              swarm_id, neg->current_round, offer->acceptance_score);

    /* Broadcast offer via bio-async */
    if (resolver->bio_context) {
        bio_message_header_t msg = {
            .type = BIO_MSG_SWARM_PROPOSAL_MADE,
            .source_module = BIO_MODULE_SWARM_MULTI,
            .target_module = 0,  /* Broadcast to all */
            .flags = BIO_MSG_FLAG_BROADCAST,
            .timestamp_us = nimcp_time_get_us()
        };

        bio_router_broadcast(resolver->bio_context, &msg, sizeof(msg));
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_accept_offer(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    uint64_t swarm_id) {

    /**
     * WHAT: Swarm accepts current offer
     * WHY:  Finalizes negotiated solution
     * HOW:  Marks conflict as resolved with accepted offer
     */

    /* Guard: Validate inputs */
    if (!resolver) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find negotiation state */
    negotiation_state_t* neg = find_negotiation(resolver, conflict_id);
    if (!neg) {
        LOG_ERROR("No active negotiation for conflict %lu", conflict_id);
        return NIMCP_NOT_FOUND;
    }

    /* Find conflict */
    conflict_t* conflict = find_active_conflict(resolver, conflict_id);
    if (!conflict) {
        LOG_ERROR("Conflict %lu not found", conflict_id);
        return NIMCP_NOT_FOUND;
    }

    /* Mark as converged */
    neg->converged = true;

    /* Create resolution result */
    resolution_result_t result = {0};
    result.conflict_id = conflict_id;
    result.strategy_used = RESOLUTION_STRATEGY_NEGOTIATION;
    result.success = true;
    result.negotiation_rounds = neg->current_round + 1;

    /* Use last offer as resolution */
    if (neg->offer_count > 0) {
        negotiation_offer_t* last_offer = &neg->offers[neg->offer_count - 1];
        memcpy(result.terms, last_offer->proposal,
               last_offer->proposal_size * sizeof(float));
        result.term_count = last_offer->proposal_size;
    }

    snprintf(result.outcome_description, sizeof(result.outcome_description),
            "Negotiation succeeded after %u rounds", result.negotiation_rounds);

    /* Mark conflict as resolved */
    conflict->is_resolved = true;
    conflict->resolution_timestamp_us = nimcp_time_get_us();

    /* Update statistics */
    resolver->stats.conflicts_resolved++;
    resolver->stats.conflicts_pending--;
    resolver->stats.negotiations_succeeded++;

    /* Move to history */
    move_to_history(resolver, conflict);

    LOG_INFO("Negotiation for conflict %lu accepted by swarm %lu after %u rounds",
             conflict_id, swarm_id, result.negotiation_rounds);

    /* Broadcast resolution */
    if (resolver->bio_context) {
        bio_message_header_t msg = {
            .type = BIO_MSG_SWARM_CONFLICT_RESOLVED,
            .source_module = BIO_MODULE_SWARM_MULTI,
            .target_module = 0,  /* Broadcast to all */
            .flags = BIO_MSG_FLAG_BROADCAST,
            .timestamp_us = nimcp_time_get_us()
        };

        bio_router_broadcast(resolver->bio_context, &msg, sizeof(msg));
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_reject_offer(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    uint64_t swarm_id,
    const char* reason) {

    /**
     * WHAT: Swarm rejects current offer
     * WHY:  Allows continued negotiation with feedback
     * HOW:  Logs rejection and increments negotiation round
     */

    /* Guard: Validate inputs */
    if (!resolver) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find negotiation state */
    negotiation_state_t* neg = find_negotiation(resolver, conflict_id);
    if (!neg) {
        LOG_ERROR("No active negotiation for conflict %lu", conflict_id);
        return NIMCP_NOT_FOUND;
    }

    /* Increment round */
    neg->current_round++;

    LOG_DEBUG("Swarm %lu rejected offer for conflict %lu (round %u): %s",
              swarm_id, conflict_id, neg->current_round,
              reason ? reason : "no reason");

    /* Check if max rounds reached */
    if (neg->current_round >= neg->max_rounds) {
        LOG_INFO("Negotiation for conflict %lu reached max rounds, timing out",
                 conflict_id);

        resolution_result_t result = {0};
        return conflict_resolver_handle_timeout(resolver, conflict_id, &result);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_check_convergence(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    bool* converged) {

    /**
     * WHAT: Determines if negotiation has converged
     * WHY:  Detect successful consensus
     * HOW:  Checks if recent offers are within threshold
     */

    /* Guard: Validate inputs */
    if (!resolver || !converged) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find negotiation state */
    negotiation_state_t* neg = find_negotiation(resolver, conflict_id);
    if (!neg) {
        *converged = false;
        return NIMCP_NOT_FOUND;
    }

    *converged = neg->converged;

    /* Check if recent offers are similar */
    if (neg->offer_count >= 2 && !neg->converged) {
        negotiation_offer_t* offer1 = &neg->offers[neg->offer_count - 2];
        negotiation_offer_t* offer2 = &neg->offers[neg->offer_count - 1];

        if (offer1->proposal_size == offer2->proposal_size) {
            float max_diff = 0.0F;
            for (uint32_t i = 0; i < offer1->proposal_size; i++) {
                float diff = fabsf(offer1->proposal[i] - offer2->proposal[i]);
                if (diff > max_diff) max_diff = diff;
            }

            if (max_diff < resolver->config.convergence_threshold) {
                neg->converged = true;
                *converged = true;
                LOG_DEBUG("Negotiation for conflict %lu converged (max_diff=%.4f)",
                         conflict_id, max_diff);
            }
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_handle_timeout(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    resolution_result_t* result) {

    /**
     * WHAT: Handles negotiation timeout scenario
     * WHY:  Prevents indefinite negotiation
     * HOW:  Falls back to arbitration or priority strategy
     */

    /* Guard: Validate inputs */
    if (!resolver || !result) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find conflict */
    conflict_t* conflict = find_active_conflict(resolver, conflict_id);
    if (!conflict) {
        LOG_ERROR("Conflict %lu not found", conflict_id);
        return NIMCP_NOT_FOUND;
    }

    LOG_INFO("Negotiation timeout for conflict %lu, falling back to arbitration",
             conflict_id);

    /* Fall back to arbitration */
    return conflict_resolver_arbitration(resolver, conflict, result);
}

/*=============================================================================
 * FEATURE 4: RESOLUTION TRACKING IMPLEMENTATION
 *============================================================================*/

nimcp_result_t conflict_resolver_get_history(
    swarm_conflict_resolver_t resolver,
    uint64_t swarm_id,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count) {

    /**
     * WHAT: Retrieves past conflicts involving a swarm
     * WHY:  Analyze conflict patterns and learn
     * HOW:  Queries history database filtered by swarm ID
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflicts || !count) {
        return NIMCP_INVALID_PARAM;
    }

    *count = 0;

    /* Search history for conflicts involving swarm */
    for (uint32_t i = 0; i < resolver->history_count && *count < max_conflicts; i++) {
        conflict_t* c = &resolver->history[i];

        /* Check if swarm is involved */
        bool involved = false;
        for (uint32_t j = 0; j < c->swarm_count; j++) {
            if (c->swarm_ids[j] == swarm_id) {
                involved = true;
                break;
            }
        }

        if (involved) {
            conflicts[*count] = *c;
            (*count)++;
        }
    }

    LOG_DEBUG("Found %u historical conflicts for swarm %lu", *count, swarm_id);

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_get_stats(
    swarm_conflict_resolver_t resolver,
    conflict_stats_t* stats) {

    /**
     * WHAT: Returns comprehensive conflict statistics
     * WHY:  Monitor system health and resolution effectiveness
     * HOW:  Aggregates stats from history
     */

    /* Guard: Validate inputs */
    if (!resolver || !stats) {
        return NIMCP_INVALID_PARAM;
    }

    /* Copy statistics */
    *stats = resolver->stats;

    /* Update pending count */
    stats->conflicts_pending = resolver->active_count;

    /* Calculate average severity from history */
    if (resolver->history_count > 0) {
        float total_severity = 0.0F;
        for (uint32_t i = 0; i < resolver->history_count; i++) {
            total_severity += resolver->history[i].severity;
        }
        stats->avg_severity = total_severity / resolver->history_count;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_get_active_conflicts(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count) {

    /**
     * WHAT: Returns currently unresolved conflicts
     * WHY:  Monitor pending conflicts
     * HOW:  Returns conflicts with is_resolved = false
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflicts || !count) {
        return NIMCP_INVALID_PARAM;
    }

    *count = 0;

    /* Copy active conflicts */
    uint32_t to_copy = resolver->active_count < max_conflicts ?
                       resolver->active_count : max_conflicts;

    memcpy(conflicts, resolver->active_conflicts, to_copy * sizeof(conflict_t));
    *count = to_copy;

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_clear_history(
    swarm_conflict_resolver_t resolver) {

    /**
     * WHAT: Clears conflict history database
     * WHY:  Reset tracking for long-running systems
     * HOW:  Clears history but retains active conflicts
     */

    /* Guard: Validate input */
    if (!resolver) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Clearing conflict history (%u entries)", resolver->history_count);

    /* Free context data */
    for (uint32_t i = 0; i < resolver->history_count; i++) {
        if (resolver->history[i].context_data) {
            nimcp_free(resolver->history[i].context_data);
            resolver->history[i].context_data = NULL;
        }
    }

    resolver->history_count = 0;

    return NIMCP_SUCCESS;
}

nimcp_result_t conflict_resolver_get_conflict(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id,
    conflict_t* conflict) {

    /**
     * WHAT: Retrieves specific conflict by ID
     * WHY:  Query detailed conflict information
     * HOW:  Searches active and historical conflicts
     */

    /* Guard: Validate inputs */
    if (!resolver || !conflict) {
        return NIMCP_INVALID_PARAM;
    }

    /* Search active conflicts */
    for (uint32_t i = 0; i < resolver->active_count; i++) {
        if (resolver->active_conflicts[i].conflict_id == conflict_id) {
            *conflict = resolver->active_conflicts[i];
            return NIMCP_SUCCESS;
        }
    }

    /* Search history */
    for (uint32_t i = 0; i < resolver->history_count; i++) {
        if (resolver->history[i].conflict_id == conflict_id) {
            *conflict = resolver->history[i];
            return NIMCP_SUCCESS;
        }
    }

    LOG_DEBUG("Conflict %lu not found", conflict_id);
    return NIMCP_NOT_FOUND;
}

nimcp_result_t conflict_resolver_analyze_patterns(
    swarm_conflict_resolver_t resolver) {

    /**
     * WHAT: Analyzes common conflict patterns
     * WHY:  Identify systemic issues and optimize
     * HOW:  Statistical analysis of conflict history
     */

    /* Guard: Validate input */
    if (!resolver) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Analyzing conflict patterns...");

    /* Print statistics */
    conflict_stats_t stats;
    conflict_resolver_get_stats(resolver, &stats);
    conflict_stats_print(&stats);

    /* Analyze common swarm pairs */
    /* (Placeholder for more sophisticated analysis) */

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

uint32_t conflict_resolver_process_inbox(
    swarm_conflict_resolver_t resolver) {

    /**
     * WHAT: Processes conflict-related bio-async messages
     * WHY:  Enables event-driven conflict resolution
     * HOW:  Handles conflict detection, negotiation, and resolution messages
     */

    /* Guard: Validate input */
    if (!resolver || !resolver->bio_context) {
        return 0;
    }

    return bio_router_process_inbox(resolver->bio_context, 0);
}

nimcp_result_t conflict_resolver_register_bioasync(
    swarm_conflict_resolver_t resolver,
    bio_router_t router) {

    /**
     * WHAT: Registers conflict resolver with bio-async router
     * WHY:  Enables message-based conflict handling
     * HOW:  Registers handlers for conflict message types
     */

    /* Guard: Validate inputs */
    if (!resolver || !router) {
        return NIMCP_INVALID_PARAM;
    }

    /* Register module */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SWARM_MULTI,
        .module_name = "swarm_conflict",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_LARGE,
        .user_data = resolver
    };

    resolver->bio_context = bio_router_register_module(&info);
    if (!resolver->bio_context) {
        LOG_ERROR("Failed to register with bio-async router");
        return NIMCP_ERROR;
    }

    LOG_INFO("Registered conflict resolver with bio-async router");

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char* conflict_type_name(conflict_type_t type) {
    switch (type) {
        case CONFLICT_TYPE_NONE:         return "NONE";
        case CONFLICT_TYPE_RESOURCE:     return "RESOURCE";
        case CONFLICT_TYPE_GOAL:         return "GOAL";
        case CONFLICT_TYPE_TERRITORY:    return "TERRITORY";
        case CONFLICT_TYPE_PRIORITY:     return "PRIORITY";
        case CONFLICT_TYPE_COMMUNICATION: return "COMMUNICATION";
        default:                         return "UNKNOWN";
    }
}

const char* resolution_strategy_name(resolution_strategy_t strategy) {
    switch (strategy) {
        case RESOLUTION_STRATEGY_PRIORITY_WINS: return "PRIORITY_WINS";
        case RESOLUTION_STRATEGY_FAIR_SHARE:    return "FAIR_SHARE";
        case RESOLUTION_STRATEGY_NEGOTIATION:   return "NEGOTIATION";
        case RESOLUTION_STRATEGY_ARBITRATION:   return "ARBITRATION";
        case RESOLUTION_STRATEGY_YIELD:         return "YIELD";
        case RESOLUTION_STRATEGY_TIME_SHARE:    return "TIME_SHARE";
        case RESOLUTION_STRATEGY_MERGE:         return "MERGE";
        default:                                return "UNKNOWN";
    }
}

void conflict_print(const conflict_t* conflict) {
    if (!conflict) return;

    printf("Conflict %lu:\n", conflict->conflict_id);
    printf("  Type: %s\n", conflict_type_name(conflict->type));
    printf("  Swarms: ");
    for (uint32_t i = 0; i < conflict->swarm_count; i++) {
        printf("%lu ", conflict->swarm_ids[i]);
    }
    printf("\n");
    printf("  Severity: %.2f\n", conflict->severity);
    printf("  Resolved: %s\n", conflict->is_resolved ? "yes" : "no");
    printf("  Description: %s\n", conflict->description);
}

void resolution_result_print(const resolution_result_t* result) {
    if (!result) return;

    printf("Resolution Result:\n");
    printf("  Conflict ID: %lu\n", result->conflict_id);
    printf("  Strategy: %s\n", resolution_strategy_name(result->strategy_used));
    printf("  Success: %s\n", result->success ? "yes" : "no");
    printf("  Winner: %lu\n", result->winner_id);
    printf("  Time: %.2f ms\n", result->resolution_time_ms);
    printf("  Outcome: %s\n", result->outcome_description);
}

void conflict_stats_print(const conflict_stats_t* stats) {
    if (!stats) return;

    printf("Conflict Statistics:\n");
    printf("  Total conflicts: %u\n", stats->total_conflicts);
    printf("  Resolved: %u\n", stats->conflicts_resolved);
    printf("  Pending: %u\n", stats->conflicts_pending);
    printf("  Failed: %u\n", stats->conflicts_failed);
    printf("  Avg resolution time: %.2f ms\n", stats->avg_resolution_time_ms);
    printf("  Max resolution time: %.2f ms\n", stats->max_resolution_time_ms);
    printf("  Min resolution time: %.2f ms\n", stats->min_resolution_time_ms);
    printf("  Avg severity: %.2f\n", stats->avg_severity);
    printf("  Negotiations started: %u\n", stats->negotiations_started);
    printf("  Negotiations succeeded: %u\n", stats->negotiations_succeeded);
    printf("  Arbitrations: %u\n", stats->arbitrations);
}

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *============================================================================*/

static nimcp_result_t add_active_conflict(
    swarm_conflict_resolver_t resolver,
    const conflict_t* conflict) {

    /* Guard: Check capacity */
    if (resolver->active_count >= resolver->active_capacity) {
        LOG_ERROR("Active conflicts capacity reached");
        return NIMCP_BUFFER_FULL;
    }

    /* Add conflict */
    resolver->active_conflicts[resolver->active_count] = *conflict;
    resolver->active_count++;

    return NIMCP_SUCCESS;
}

static nimcp_result_t move_to_history(
    swarm_conflict_resolver_t resolver,
    conflict_t* conflict) {

    /* Guard: Check capacity */
    if (resolver->history_count >= resolver->history_capacity) {
        /* Overwrite oldest entry */
        if (resolver->history[0].context_data) {
            nimcp_free(resolver->history[0].context_data);
        }
        memmove(&resolver->history[0], &resolver->history[1],
                (resolver->history_capacity - 1) * sizeof(conflict_t));
        resolver->history_count--;
    }

    /* Add to history */
    resolver->history[resolver->history_count] = *conflict;
    resolver->history_count++;

    /* Remove from active */
    for (uint32_t i = 0; i < resolver->active_count; i++) {
        if (resolver->active_conflicts[i].conflict_id == conflict->conflict_id) {
            memmove(&resolver->active_conflicts[i],
                   &resolver->active_conflicts[i + 1],
                   (resolver->active_count - i - 1) * sizeof(conflict_t));
            resolver->active_count--;
            break;
        }
    }

    return NIMCP_SUCCESS;
}

static negotiation_state_t* find_negotiation(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id) {

    for (uint32_t i = 0; i < resolver->negotiation_count; i++) {
        if (resolver->negotiations[i].conflict_id == conflict_id) {
            return &resolver->negotiations[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

static conflict_t* find_active_conflict(
    swarm_conflict_resolver_t resolver,
    uint64_t conflict_id) {

    for (uint32_t i = 0; i < resolver->active_count; i++) {
        if (resolver->active_conflicts[i].conflict_id == conflict_id) {
            return &resolver->active_conflicts[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

static bool territories_overlap(
    const nimcp_territory_bounds_t* a,
    const nimcp_territory_bounds_t* b) {

    /* Check 3D bounding box intersection */
    return !(a->max.x < b->min.x || a->min.x > b->max.x ||
             a->max.y < b->min.y || a->min.y > b->max.y ||
             a->max.z < b->min.z || a->min.z > b->max.z);
}

static float calculate_territory_overlap(
    const nimcp_territory_bounds_t* a,
    const nimcp_territory_bounds_t* b) {

    /* Calculate overlap volume as fraction of smaller territory */
    if (!territories_overlap(a, b)) {
        return 0.0F;
    }

    /* Calculate intersection volume */
    float ix = fminf(a->max.x, b->max.x) - fmaxf(a->min.x, b->min.x);
    float iy = fminf(a->max.y, b->max.y) - fmaxf(a->min.y, b->min.y);
    float iz = fminf(a->max.z, b->max.z) - fmaxf(a->min.z, b->min.z);

    if (ix < 0 || iy < 0 || iz < 0) return 0.0F;

    float intersection = ix * iy * iz;

    /* Calculate volumes */
    float vol_a = (a->max.x - a->min.x) * (a->max.y - a->min.y) * (a->max.z - a->min.z);
    float vol_b = (b->max.x - b->min.x) * (b->max.y - b->min.y) * (b->max.z - b->min.z);

    float smaller_vol = fminf(vol_a, vol_b);

    if (smaller_vol < 0.0001F) return 0.0F;

    return intersection / smaller_vol;
}
