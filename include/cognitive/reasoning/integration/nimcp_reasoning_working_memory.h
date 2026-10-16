/**
 * @file nimcp_reasoning_working_memory.h
 * @brief MODULE 3: Reasoning-Working Memory Integration
 * @version 1.0.0
 * @date 2025-11-20
 *
 * SOLE RESPONSIBILITY: Store active inferences in working memory (7±2 limit)
 *
 * WHAT: Integration module managing active inference storage in WM buffer
 * WHY:  Multi-step reasoning requires maintaining context (Miller's 7±2 limit)
 * HOW:  Subscribe to inference events, manage WM slots with salience-based eviction
 *
 * BIOLOGICAL BASIS:
 * - Dorsolateral prefrontal cortex maintains WM representations
 * - 7±2 capacity limit (Miller, 1956)
 * - Decay over time without rehearsal
 */

#ifndef NIMCP_REASONING_WORKING_MEMORY_H
#define NIMCP_REASONING_WORKING_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reasoning_working_memory reasoning_working_memory_t;

#define REASONING_WM_MAX_INFERENCES 7        /**< Miller's 7±2 limit */
#define REASONING_WM_DECAY_TAU_MS   10000    /**< 10 second decay constant */

typedef struct {
    char goal[256];
    uint32_t step_count;
    uint64_t start_time_ms;
    float salience;
    bool is_active;
    uint32_t inference_id;
} wm_inference_t;

typedef struct {
    uint32_t max_active_inferences;
    uint32_t decay_tau_ms;
    float min_salience_for_storage;
    bool enable_salience_decay;
} reasoning_wm_config_t;

typedef struct {
    uint64_t total_events_processed;
    uint64_t inferences_stored;
    uint64_t inferences_evicted;
    uint32_t current_wm_count;
    float avg_wm_utilization;
    uint64_t avg_callback_time_us;
} reasoning_wm_stats_t;

// Lifecycle
reasoning_working_memory_t* reasoning_wm_create(event_bus_t bus);
reasoning_working_memory_t* reasoning_wm_create_custom(event_bus_t bus, const reasoning_wm_config_t* config);
void reasoning_wm_destroy(reasoning_working_memory_t* integration);

// Core
void reasoning_wm_callback(const brain_event_t* event, void* context);
bool reasoning_wm_store_inference(reasoning_working_memory_t* integration, const wm_inference_t* inference);
uint32_t reasoning_wm_get_active_inferences(const reasoning_working_memory_t* integration, wm_inference_t* inferences, uint32_t max_count);

// Query
bool reasoning_wm_get_config(const reasoning_working_memory_t* integration, reasoning_wm_config_t* config);
bool reasoning_wm_set_config(reasoning_working_memory_t* integration, const reasoning_wm_config_t* config);
bool reasoning_wm_get_stats(const reasoning_working_memory_t* integration, reasoning_wm_stats_t* stats);
bool reasoning_wm_reset_stats(reasoning_working_memory_t* integration);

// Utility
reasoning_wm_config_t reasoning_wm_default_config(void);
bool reasoning_wm_validate_config(const reasoning_wm_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_WORKING_MEMORY_H
