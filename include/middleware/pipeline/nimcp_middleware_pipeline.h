//=============================================================================
// nimcp_middleware_pipeline.h - Complete Middleware Pipeline System
//=============================================================================

#ifndef NIMCP_MIDDLEWARE_PIPELINE_H
#define NIMCP_MIDDLEWARE_PIPELINE_H

#include "middleware/pipeline/nimcp_middleware_context.h"
#include "core/events/nimcp_event_bus.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_middleware_pipeline.h
 * @brief Complete middleware processing pipeline for NIMCP
 *
 * WHAT: Multi-stage pipeline for neural signal processing
 * WHY:  Structured, extensible processing from spikes to cognition
 * HOW:  Stage-based execution with event bus integration
 *
 * PIPELINE STAGES:
 * 1. ENCODING     - Rate encoding from spikes
 * 2. EXTRACTION   - Feature extraction
 * 3. DETECTION    - Pattern detection
 * 4. ROUTING      - Thalamic routing
 * 5. NORMALIZATION - Feature normalization
 * 6. BUFFERING    - Temporal buffering
 * 7. EVENTS       - Event generation and dispatch
 */

//=============================================================================
// Pipeline Types
//=============================================================================

typedef struct middleware_pipeline_struct* middleware_pipeline_t;

/**
 * @brief Pipeline stage enumeration
 */
typedef enum {
    PIPELINE_STAGE_ENCODING = 0,
    PIPELINE_STAGE_EXTRACTION,
    PIPELINE_STAGE_DETECTION,
    PIPELINE_STAGE_ROUTING,
    PIPELINE_STAGE_NORMALIZATION,
    PIPELINE_STAGE_BUFFERING,
    PIPELINE_STAGE_EVENTS,
    PIPELINE_STAGE_COUNT
} pipeline_stage_id_t;

/**
 * @brief Pipeline stage function
 *
 * @param context Shared context
 * @param stage_data Stage-specific configuration
 * @return true on success
 */
typedef bool (*pipeline_stage_fn)(middleware_context_t* context, void* stage_data);

/**
 * @brief Pipeline stage configuration
 */
typedef struct {
    pipeline_stage_id_t id;
    const char* name;
    pipeline_stage_fn execute;
    void* stage_data;
    bool enabled;
    uint32_t timeout_us;
} pipeline_stage_config_t;

/**
 * @brief Pipeline configuration
 */
typedef struct {
    pipeline_stage_config_t* stages;
    uint32_t num_stages;
    event_bus_t event_bus;
    bool enable_profiling;
    bool fail_fast;  // Stop on first error?
    bool enable_bio_async;  /**< Enable bio-async integration */
} pipeline_config_t;

/**
 * @brief Pipeline statistics
 */
typedef struct {
    uint64_t total_executions;
    uint64_t successful_executions;
    uint64_t failed_executions;
    uint64_t* stage_execution_counts;
    uint64_t* stage_total_time_us;
    float* stage_avg_time_us;
    uint32_t num_stages;
} pipeline_stats_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create middleware pipeline
 *
 * @param config Pipeline configuration
 * @return Pipeline handle or NULL on error
 */
middleware_pipeline_t middleware_pipeline_create(const pipeline_config_t* config);

/**
 * @brief Destroy pipeline
 */
void middleware_pipeline_destroy(middleware_pipeline_t pipeline);

//=============================================================================
// Execution
//=============================================================================

/**
 * @brief Execute complete pipeline
 *
 * WHAT: Run all enabled stages in sequence
 * WHY:  Process neural activity through full middleware
 * HOW:  Iterate stages, execute each, accumulate results
 *
 * COMPLEXITY: O(S) where S = number of stages
 * THREAD-SAFE: Yes (if context is not shared)
 *
 * @param pipeline Pipeline handle
 * @param context Execution context
 * @return true if all stages succeeded
 */
bool middleware_pipeline_execute(middleware_pipeline_t pipeline,
                                 middleware_context_t* context);

/**
 * @brief Execute single stage
 *
 * @param pipeline Pipeline handle
 * @param stage_id Stage to execute
 * @param context Execution context
 * @return true on success
 */
bool middleware_pipeline_execute_stage(middleware_pipeline_t pipeline,
                                       pipeline_stage_id_t stage_id,
                                       middleware_context_t* context);

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Enable/disable pipeline stage
 *
 * @param pipeline Pipeline handle
 * @param stage_id Stage to configure
 * @param enabled Enable flag
 * @return true on success
 */
bool middleware_pipeline_set_stage_enabled(middleware_pipeline_t pipeline,
                                           pipeline_stage_id_t stage_id,
                                           bool enabled);

/**
 * @brief Get pipeline statistics
 */
bool middleware_pipeline_get_stats(middleware_pipeline_t pipeline,
                                   pipeline_stats_t* stats);

/**
 * @brief Free memory allocated by middleware_pipeline_get_stats()
 *
 * P2-MW-10 fix: get_stats allocates stage_execution_counts, stage_total_time_us,
 * and stage_avg_time_us arrays. Callers MUST call this to free those arrays.
 */
void middleware_pipeline_stats_free(pipeline_stats_t* stats);

/**
 * @brief Reset pipeline statistics
 */
void middleware_pipeline_reset_stats(middleware_pipeline_t pipeline);

//=============================================================================
// Default Pipeline
//=============================================================================

/**
 * @brief Create default NIMCP middleware pipeline
 *
 * WHAT: Pre-configured pipeline with all standard stages
 * WHY:  Quick setup for typical use cases
 * HOW:  Creates encoding → extraction → detection → routing → norm → buffer → events
 *
 * @param brain Brain reference
 * @param event_bus Event bus for event generation
 * @return Configured pipeline ready for execution
 */
middleware_pipeline_t middleware_pipeline_create_default(brain_t brain,
                                                         event_bus_t event_bus);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIDDLEWARE_PIPELINE_H
