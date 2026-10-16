/**
 * @file nimcp_reasoning_curiosity.h
 * @brief MODULE 2: Reasoning-Curiosity Integration
 * @version 1.0.0
 * @date 2025-11-20
 *
 * SOLE RESPONSIBILITY: Trigger curiosity-driven exploration of unexplained facts
 *
 * WHAT: Integration module connecting proof failures to curiosity system
 * WHY:  Unexplained facts should trigger exploration and knowledge seeking
 * HOW:  Subscribe to proof failure events, trigger curiosity-driven exploration
 *
 * BIOLOGICAL BASIS:
 * - Prediction errors drive exploratory behavior (dopamine dip)
 * - Anterior cingulate cortex signals uncertainty
 * - Curiosity system motivated by information gaps
 */

#ifndef NIMCP_REASONING_CURIOSITY_H
#define NIMCP_REASONING_CURIOSITY_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct curiosity_engine_struct* curiosity_engine_t;
typedef struct reasoning_curiosity reasoning_curiosity_t;

#define REASONING_CURIOSITY_PROOF_FAILED_BOOST   0.3f  /**< Curiosity boost for proof failures */
#define REASONING_CURIOSITY_NOVEL_FACT_BOOST     0.1f  /**< Curiosity boost for novel facts */

typedef struct {
    bool enable_proof_failure_exploration;
    bool enable_novel_fact_exploration;
    float proof_failed_curiosity_boost;
    float novel_fact_curiosity_boost;
    float min_curiosity_threshold;
} reasoning_curiosity_config_t;

typedef struct {
    uint64_t total_events_processed;
    uint64_t exploration_triggers;
    uint64_t proof_failure_triggers;
    uint64_t novel_fact_triggers;
    float avg_curiosity_boost;
    uint64_t avg_callback_time_us;
} reasoning_curiosity_stats_t;

// Lifecycle
reasoning_curiosity_t* reasoning_curiosity_create(event_bus_t bus, curiosity_engine_t curiosity);
reasoning_curiosity_t* reasoning_curiosity_create_custom(event_bus_t bus, curiosity_engine_t curiosity, const reasoning_curiosity_config_t* config);
void reasoning_curiosity_destroy(reasoning_curiosity_t* integration);

// Core
void reasoning_curiosity_callback(const brain_event_t* event, void* context);
bool reasoning_curiosity_explore_unexplained_fact(reasoning_curiosity_t* integration, const char* fact);

// Query
bool reasoning_curiosity_get_config(const reasoning_curiosity_t* integration, reasoning_curiosity_config_t* config);
bool reasoning_curiosity_set_config(reasoning_curiosity_t* integration, const reasoning_curiosity_config_t* config);
bool reasoning_curiosity_get_stats(const reasoning_curiosity_t* integration, reasoning_curiosity_stats_t* stats);
bool reasoning_curiosity_reset_stats(reasoning_curiosity_t* integration);

// Utility
reasoning_curiosity_config_t reasoning_curiosity_default_config(void);
bool reasoning_curiosity_validate_config(const reasoning_curiosity_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_CURIOSITY_H
