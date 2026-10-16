/**
 * @file nimcp_reasoning_consolidation.h
 * @brief MODULE 5: Reasoning-Consolidation Integration
 * @version 1.0.0
 * @date 2025-11-20
 *
 * SOLE RESPONSIBILITY: Consolidate important rules to long-term memory
 *
 * WHAT: Integration module managing rule consolidation to LTM
 * WHY:  Frequently-used rules should be consolidated for efficiency
 * HOW:  Track rule usage, consolidate when threshold exceeded
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus-neocortex consolidation during sleep
 * - Replay of important patterns strengthens memories
 * - Consolidation threshold based on importance
 */

#ifndef NIMCP_REASONING_CONSOLIDATION_H
#define NIMCP_REASONING_CONSOLIDATION_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct consolidation_handle_struct* consolidation_handle_t;
typedef struct reasoning_consolidation reasoning_consolidation_t;

#define REASONING_CONSOL_MIN_USES 5           /**< Min uses before consolidation */
#define REASONING_CONSOL_IMPORTANCE_THRESHOLD 0.6f

typedef struct {
    char rule[512];
    uint32_t use_count;
    uint32_t success_count;
    float importance;
    uint64_t first_used_ms;
    uint64_t last_used_ms;
    bool consolidated;
} rule_usage_t;

typedef struct {
    uint32_t min_rule_uses_for_consolidation;
    float consolidation_threshold;
    bool enable_frequency_based_consolidation;
    bool enable_importance_based_consolidation;
    uint32_t max_tracked_rules;
} reasoning_consolidation_config_t;

typedef struct {
    uint64_t total_events_processed;
    uint64_t rules_tracked;
    uint64_t rules_consolidated;
    float avg_rule_importance;
    uint32_t current_tracked_count;
    uint64_t avg_callback_time_us;
} reasoning_consolidation_stats_t;

// Lifecycle
reasoning_consolidation_t* reasoning_consolidation_create(event_bus_t bus, consolidation_handle_t consolidation);
reasoning_consolidation_t* reasoning_consolidation_create_custom(event_bus_t bus, consolidation_handle_t consolidation, const reasoning_consolidation_config_t* config);
void reasoning_consolidation_destroy(reasoning_consolidation_t* integration);

// Core
void reasoning_consolidation_callback(const brain_event_t* event, void* context);
bool reasoning_consolidation_consolidate_rule(reasoning_consolidation_t* integration, const rule_usage_t* rule);
uint32_t reasoning_consolidation_get_tracked_rules(const reasoning_consolidation_t* integration, rule_usage_t* rules, uint32_t max_count);

// Query
bool reasoning_consolidation_get_config(const reasoning_consolidation_t* integration, reasoning_consolidation_config_t* config);
bool reasoning_consolidation_set_config(reasoning_consolidation_t* integration, const reasoning_consolidation_config_t* config);
bool reasoning_consolidation_get_stats(const reasoning_consolidation_t* integration, reasoning_consolidation_stats_t* stats);
bool reasoning_consolidation_reset_stats(reasoning_consolidation_t* integration);

// Utility
reasoning_consolidation_config_t reasoning_consolidation_default_config(void);
bool reasoning_consolidation_validate_config(const reasoning_consolidation_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_CONSOLIDATION_H
