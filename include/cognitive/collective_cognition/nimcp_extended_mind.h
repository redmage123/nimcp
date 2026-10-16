/**
 * @file nimcp_extended_mind.h
 * @brief External tools as cognitive extensions
 *
 * WHAT: Model external tools (MCP servers, databases, AI models) as cognitive extensions
 * WHY: Extended cognition - tools become part of the cognitive system
 * HOW: Track tool reliability, latency, and integration depth
 *
 * THEORETICAL BASIS:
 * - Extended Mind Thesis (Clark & Chalmers, 1998): Cognitive processes can extend
 *   beyond the brain to include external artifacts that meet certain criteria
 * - Criteria for extension: Reliable, available, readily accessible, automatically
 *   endorsed, and deeply integrated into cognitive processes
 *
 * EXTENSION TYPES:
 * - Memory: External databases, files, knowledge bases
 * - Perception: External sensors, cameras, APIs
 * - Reasoning: LLMs, calculators, theorem provers
 * - Action: Robots, actuators, API calls
 * - Communication: Messaging systems, notifications
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_EXTENDED_MIND_H
#define NIMCP_EXTENDED_MIND_H

#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Maximum extension name length */
#define EXT_NAME_MAX_LEN                64

/** Maximum query payload size */
#define EXT_QUERY_MAX_SIZE              4096

/** Maximum response payload size */
#define EXT_RESPONSE_MAX_SIZE           16384

/** Query timeout in milliseconds */
#define EXT_QUERY_TIMEOUT_MS            5000

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Query status
 */
typedef enum {
    EXT_QUERY_PENDING = 0,
    EXT_QUERY_IN_PROGRESS,
    EXT_QUERY_COMPLETED,
    EXT_QUERY_FAILED,
    EXT_QUERY_TIMEOUT,
    EXT_QUERY_CANCELLED
} ext_query_status_t;

/**
 * @brief Extension health status
 */
typedef enum {
    EXT_HEALTH_UNKNOWN = 0,
    EXT_HEALTH_HEALTHY,
    EXT_HEALTH_DEGRADED,
    EXT_HEALTH_UNAVAILABLE
} ext_health_t;

/**
 * @brief Query callback function type
 */
typedef void (*ext_query_callback_fn)(
    uint32_t query_id,
    ext_query_status_t status,
    const void* response,
    size_t response_size,
    void* user_data
);

/**
 * @brief Extension query function type
 *
 * The extension implements this to handle queries.
 */
typedef int (*ext_query_fn)(
    const void* query,
    size_t query_size,
    void* response,
    size_t* response_size,
    void* ext_user_data
);

/**
 * @brief Extension status function type
 *
 * The extension implements this to report its status.
 */
typedef ext_health_t (*ext_status_fn)(void* ext_user_data);

/**
 * @brief Cognitive extension descriptor
 */
typedef struct {
    uint32_t extension_id;
    extension_type_t type;
    char name[EXT_NAME_MAX_LEN];

    /* Integration quality metrics */
    float reliability;          /**< Historical success rate [0-1] */
    float avg_latency_ms;       /**< Average response time */
    float integration_depth;    /**< How tightly coupled [0-1] */
    float trust_level;          /**< Epistemic trust [0-1] */

    /* Current state */
    ext_health_t health;
    float current_load;         /**< Current utilization [0-1] */

    /* Usage statistics */
    uint64_t total_queries;
    uint64_t successful_queries;
    uint64_t failed_queries;
    uint64_t last_access_us;
    uint64_t last_failure_us;

    /* Callbacks */
    ext_query_fn query_fn;
    ext_status_fn status_fn;
    void* user_data;
} cognitive_extension_t;

/**
 * @brief Query request
 */
typedef struct {
    uint32_t query_id;
    uint32_t requester_id;      /**< Brain instance requesting */
    extension_type_t type;      /**< Type of extension to query */
    uint32_t extension_id;      /**< Specific extension (0 = any of type) */
    float priority;             /**< Query priority [0-1] */
    uint32_t timeout_ms;        /**< Timeout in milliseconds */
    ext_query_callback_fn callback;
    void* callback_user_data;
} ext_query_request_t;

/**
 * @brief Offload request for task delegation
 */
typedef struct {
    uint32_t offload_id;
    uint32_t source_instance;   /**< Source brain instance */
    extension_type_t type;      /**< Type of extension for offload */
    float estimated_load;       /**< Estimated computational load */
    uint32_t deadline_ms;       /**< Deadline for completion */
} ext_offload_request_t;

/**
 * @brief Extended mind statistics
 */
typedef struct {
    uint64_t total_queries;
    uint64_t successful_queries;
    uint64_t failed_queries;
    uint64_t timeout_queries;
    uint64_t offload_requests;
    uint64_t offload_completions;
    float avg_latency_ms;
    float avg_reliability;
    uint64_t bytes_transferred;
} extended_mind_stats_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create extended mind system
 *
 * @param config Configuration (NULL for defaults)
 * @return Extended mind handle or NULL on failure
 */
extended_mind_t* extended_mind_create(const extended_mind_config_t* config);

/**
 * @brief Destroy extended mind system
 *
 * @param em Extended mind system to destroy
 */
void extended_mind_destroy(extended_mind_t* em);

/**
 * @brief Reset extended mind system
 *
 * @param em Extended mind system
 * @return 0 on success, -1 on error
 */
int extended_mind_reset(extended_mind_t* em);

/*=============================================================================
 * Extension Management API
 *===========================================================================*/

/**
 * @brief Register a cognitive extension
 *
 * @param em Extended mind system
 * @param ext Extension descriptor
 * @return Extension ID on success, 0 on failure
 */
uint32_t extended_mind_register_extension(
    extended_mind_t* em,
    const cognitive_extension_t* ext
);

/**
 * @brief Unregister a cognitive extension
 *
 * @param em Extended mind system
 * @param extension_id Extension to unregister
 * @return 0 on success, -1 on error
 */
int extended_mind_unregister_extension(
    extended_mind_t* em,
    uint32_t extension_id
);

/**
 * @brief Get extension information
 *
 * @param em Extended mind system
 * @param extension_id Extension ID
 * @param ext Output extension descriptor
 * @return 0 on success, -1 on error
 */
int extended_mind_get_extension(
    const extended_mind_t* em,
    uint32_t extension_id,
    cognitive_extension_t* ext
);

/**
 * @brief Update extension statistics after a query
 *
 * @param em Extended mind system
 * @param extension_id Extension ID
 * @param success Whether query succeeded
 * @param latency_ms Query latency
 * @return 0 on success, -1 on error
 */
int extended_mind_update_extension_stats(
    extended_mind_t* em,
    uint32_t extension_id,
    bool success,
    float latency_ms
);

/**
 * @brief Get number of registered extensions
 *
 * @param em Extended mind system
 * @return Number of extensions
 */
uint32_t extended_mind_extension_count(const extended_mind_t* em);

/**
 * @brief Get number of extensions of a specific type
 *
 * @param em Extended mind system
 * @param type Extension type
 * @return Number of extensions of that type
 */
uint32_t extended_mind_count_by_type(
    const extended_mind_t* em,
    extension_type_t type
);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Submit a query to an extension
 *
 * @param em Extended mind system
 * @param request Query request
 * @param query Query data
 * @param query_size Query data size
 * @return Query ID on success, 0 on failure
 */
uint32_t extended_mind_query(
    extended_mind_t* em,
    const ext_query_request_t* request,
    const void* query,
    size_t query_size
);

/**
 * @brief Submit a synchronous query (blocking)
 *
 * @param em Extended mind system
 * @param type Extension type
 * @param query Query data
 * @param query_size Query data size
 * @param response Output response buffer
 * @param response_size Input: buffer size, Output: response size
 * @return 0 on success, -1 on error
 */
int extended_mind_query_sync(
    extended_mind_t* em,
    extension_type_t type,
    const void* query,
    size_t query_size,
    void* response,
    size_t* response_size
);

/**
 * @brief Cancel a pending query
 *
 * @param em Extended mind system
 * @param query_id Query to cancel
 * @return 0 on success, -1 on error
 */
int extended_mind_cancel_query(
    extended_mind_t* em,
    uint32_t query_id
);

/**
 * @brief Get query status
 *
 * @param em Extended mind system
 * @param query_id Query ID
 * @return Query status
 */
ext_query_status_t extended_mind_get_query_status(
    const extended_mind_t* em,
    uint32_t query_id
);

/*=============================================================================
 * Offload API
 *===========================================================================*/

/**
 * @brief Offload a cognitive task to an extension
 *
 * @param em Extended mind system
 * @param request Offload request
 * @param task Task data
 * @param task_size Task data size
 * @return Offload ID on success, 0 on failure
 */
uint32_t extended_mind_offload(
    extended_mind_t* em,
    const ext_offload_request_t* request,
    const void* task,
    size_t task_size
);

/**
 * @brief Check offload completion
 *
 * @param em Extended mind system
 * @param offload_id Offload ID
 * @param result Output result buffer
 * @param result_size Input: buffer size, Output: result size
 * @return 0 if complete, 1 if pending, -1 on error
 */
int extended_mind_check_offload(
    extended_mind_t* em,
    uint32_t offload_id,
    void* result,
    size_t* result_size
);

/*=============================================================================
 * State API
 *===========================================================================*/

/**
 * @brief Get extended mind state
 *
 * @param em Extended mind system
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int extended_mind_get_state(
    const extended_mind_t* em,
    extended_mind_state_t* state
);

/**
 * @brief Get total cognitive capacity across all extensions
 *
 * @param em Extended mind system
 * @param type Extension type (or -1 for all)
 * @return Capacity measure
 */
float extended_mind_get_capacity(
    const extended_mind_t* em,
    extension_type_t type
);

/**
 * @brief Get best available extension for a type
 *
 * @param em Extended mind system
 * @param type Extension type
 * @return Extension ID, or 0 if none available
 */
uint32_t extended_mind_best_extension(
    const extended_mind_t* em,
    extension_type_t type
);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update extended mind state
 *
 * Checks extension health, processes pending queries, updates metrics.
 *
 * @param em Extended mind system
 * @return 0 on success, -1 on error
 */
int extended_mind_update(extended_mind_t* em);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get extended mind statistics
 *
 * @param em Extended mind system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int extended_mind_get_stats(
    const extended_mind_t* em,
    extended_mind_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param em Extended mind system
 */
void extended_mind_reset_stats(extended_mind_t* em);

/*=============================================================================
 * Debug API
 *===========================================================================*/

/**
 * @brief Dump extended mind state for debugging
 *
 * @param em Extended mind system
 */
void extended_mind_dump(const extended_mind_t* em);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXTENDED_MIND_H */
