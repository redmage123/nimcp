/**
 * @file nimcp_rcog_tool_router.h
 * @brief Recursive Cognition Tool Router - Capability-Based Tool Access Control
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Routes tool invocations to registered handlers based on capability tiers
 * WHY:  Maps to RLM's tiered tool access where root has NO tools
 * HOW:  Registry of tools by tier, access control enforcement, execution routing
 *
 * BIOLOGICAL BASIS:
 * The tool router models the hierarchical organization of motor and cognitive control:
 * - Prefrontal (root): Planning and coordination, no direct motor control
 * - Premotor (L1): High-level action planning
 * - Motor cortex (L2): Sensory-motor integration
 * - Primary motor (L3): Direct motor commands
 * - Specialized areas (L4): Domain-specific processing (visual, auditory, etc.)
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+     +----------------------+
 * |   DELEGATION POOL    |     |    TOOL ROUTER       |
 * |                      |     |                      |
 * | execute_task(tier,   |---->|  Access Control      |
 * |   tool_name, input)  |     |  (tier check)        |
 * |                      |     |                      |
 * |                      |<----|  Route to Handler    |
 * +----------------------+     +----------+-----------+
 *                                         |
 *          +------------------------------+------------------------------+
 *          |              |               |               |              |
 *    +-----v-----+  +-----v-----+   +-----v-----+   +-----v-----+  +-----v-----+
 *    | L1 Tools  |  | L2 Tools  |   | L3 Tools  |   | L4 Tools  |  | Custom    |
 *    | (Memory,  |  | (Sensor,  |   | (Motor,   |   | (Vision,  |  | Tools     |
 *    |  Logic)   |  |  Feature) |   |  Output)  |   |  Audio)   |  |           |
 *    +-----------+  +-----------+   +-----------+   +-----------+  +-----------+
 * ```
 *
 * KEY CONCEPTS:
 * - Tier Hierarchy: Higher tiers can access lower tier tools (L4 can use L1-L3)
 * - Root Isolation: RCOG_TIER_ROOT has NO tool access (coordination only)
 * - Tool Categories: Logical groupings within tiers
 * - Access Policies: Fine-grained control beyond tier (rate limits, quotas)
 * - Audit Trail: Complete logging of tool invocations
 */

#ifndef NIMCP_RCOG_TOOL_ROUTER_H
#define NIMCP_RCOG_TOOL_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_context_store;
struct rcog_bio_async_bridge;
struct rcog_immune_bridge;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum registered tools */
#define RCOG_ROUTER_MAX_TOOLS 256

/** Maximum tool name length */
#define RCOG_ROUTER_MAX_TOOL_NAME 64

/** Maximum tool description length */
#define RCOG_ROUTER_MAX_TOOL_DESC 256

/** Maximum tool categories */
#define RCOG_ROUTER_MAX_CATEGORIES 32

/** Maximum category name length */
#define RCOG_ROUTER_MAX_CATEGORY_NAME 32

/** Maximum tools per category */
#define RCOG_ROUTER_MAX_TOOLS_PER_CATEGORY 32

/** Default tool timeout (ms) */
#define RCOG_ROUTER_DEFAULT_TOOL_TIMEOUT_MS 10000

/** Maximum concurrent tool executions */
#define RCOG_ROUTER_MAX_CONCURRENT 64

/*=============================================================================
 * TOOL DEFINITION
 *===========================================================================*/

/**
 * @brief Tool input/output type hints
 */
typedef enum {
    RCOG_TOOL_IO_ANY = 0,        /**< Any type accepted */
    RCOG_TOOL_IO_TEXT,           /**< UTF-8 text */
    RCOG_TOOL_IO_JSON,           /**< JSON-formatted */
    RCOG_TOOL_IO_BINARY,         /**< Raw binary */
    RCOG_TOOL_IO_TENSOR,         /**< NIMCP tensor */
    RCOG_TOOL_IO_EMBEDDING,      /**< Vector embedding */
    RCOG_TOOL_IO_GRAPH           /**< Knowledge graph */
} rcog_tool_io_type_t;

/**
 * @brief Tool execution mode
 */
typedef enum {
    RCOG_TOOL_SYNC = 0,          /**< Synchronous execution */
    RCOG_TOOL_ASYNC,             /**< Asynchronous (returns handle) */
    RCOG_TOOL_STREAMING          /**< Streaming output */
} rcog_tool_exec_mode_t;

/**
 * @brief Tool flags
 */
typedef enum {
    RCOG_TOOL_FLAG_NONE = 0,
    RCOG_TOOL_FLAG_STATEFUL = (1 << 0),     /**< Maintains state */
    RCOG_TOOL_FLAG_IDEMPOTENT = (1 << 1),   /**< Same input = same output */
    RCOG_TOOL_FLAG_SIDE_EFFECTS = (1 << 2), /**< Has side effects */
    RCOG_TOOL_FLAG_EXPENSIVE = (1 << 3),    /**< Resource intensive */
    RCOG_TOOL_FLAG_PRIVILEGED = (1 << 4),   /**< Requires extra auth */
    RCOG_TOOL_FLAG_DEPRECATED = (1 << 5),   /**< Scheduled for removal */
    RCOG_TOOL_FLAG_EXPERIMENTAL = (1 << 6)  /**< Unstable API */
} rcog_tool_flags_t;

/**
 * @brief Tool parameter definition
 */
typedef struct {
    char name[32];               /**< Parameter name */
    rcog_tool_io_type_t type;    /**< Expected type */
    bool required;               /**< Required parameter? */
    const char* description;     /**< Human-readable description */
    const char* default_value;   /**< Default value (JSON) */
} rcog_tool_param_t;

/**
 * @brief Tool definition for registration
 */
typedef struct {
    char name[RCOG_ROUTER_MAX_TOOL_NAME];
    char description[RCOG_ROUTER_MAX_TOOL_DESC];
    char category[RCOG_ROUTER_MAX_CATEGORY_NAME];

    rcog_capability_tier_t min_tier;  /**< Minimum tier required */
    rcog_tool_exec_mode_t exec_mode;
    rcog_tool_flags_t flags;

    rcog_tool_io_type_t input_type;
    rcog_tool_io_type_t output_type;

    rcog_tool_fn handler;        /**< Execution handler */
    void* context;               /**< Tool-specific context */

    /* Optional schema */
    rcog_tool_param_t* params;   /**< Parameter definitions */
    size_t num_params;

    /* Limits */
    uint32_t timeout_ms;         /**< Execution timeout */
    uint32_t rate_limit_per_sec; /**< Max calls per second (0 = unlimited) */
    size_t max_input_size;       /**< Max input size (0 = unlimited) */
    size_t max_output_size;      /**< Max output size (0 = unlimited) */
} rcog_tool_def_t;

/*=============================================================================
 * TOOL INVOCATION
 *===========================================================================*/

/**
 * @brief Tool invocation request
 */
typedef struct {
    const char* tool_name;       /**< Tool to invoke */
    rcog_capability_tier_t caller_tier; /**< Caller's tier */
    const void* input;           /**< Input data */
    size_t input_size;           /**< Input size */
    rcog_tool_io_type_t input_type; /**< Input type hint */
    uint32_t timeout_ms;         /**< Override timeout (0 = use default) */
    void* user_data;             /**< Passed to callback */
} rcog_tool_request_t;

/**
 * @brief Tool invocation result
 */
typedef struct {
    const char* tool_name;       /**< Tool that was invoked */
    rcog_error_t error;          /**< Error code */
    bool success;                /**< True if successful */
    void* output;                /**< Output data (caller frees) */
    size_t output_size;          /**< Output size */
    rcog_tool_io_type_t output_type; /**< Actual output type */
    uint64_t duration_ms;        /**< Execution duration */
    const char* error_message;   /**< Error details if failed */
} rcog_tool_result_t;

/**
 * @brief Async tool completion callback
 */
typedef void (*rcog_tool_callback_t)(
    const rcog_tool_result_t* result,
    void* user_data
);

/**
 * @brief Streaming output callback
 */
typedef void (*rcog_tool_stream_callback_t)(
    const void* chunk,
    size_t chunk_size,
    bool is_final,
    void* user_data
);

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Access policy for fine-grained control
 */
typedef struct {
    bool allow_cross_tier;       /**< Allow lower tier to use higher tier tools */
    bool require_auth_token;     /**< Require authentication token */
    uint32_t global_rate_limit;  /**< Global rate limit (0 = unlimited) */
    uint32_t max_concurrent;     /**< Max concurrent executions */
    bool audit_all_calls;        /**< Log all tool invocations */
    bool allow_experimental;     /**< Allow experimental tools */
} rcog_access_policy_t;

/**
 * @brief Router configuration
 */
typedef struct {
    /* Access control */
    rcog_access_policy_t access_policy;

    /* Execution */
    uint32_t default_timeout_ms;
    size_t default_max_input_size;
    size_t default_max_output_size;
    bool enable_async;
    bool enable_streaming;

    /* Integration */
    bool enable_bio_async;       /**< Neuromodulator effects on tool execution */
    bool enable_immune_check;    /**< Check immune status before execution */

    /* Monitoring */
    bool enable_metrics;
    bool enable_tracing;
    bool verbose_logging;
} rcog_tool_router_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Per-tool statistics
 */
typedef struct {
    char tool_name[RCOG_ROUTER_MAX_TOOL_NAME];
    uint64_t invocations;
    uint64_t successes;
    uint64_t failures;
    uint64_t timeouts;
    uint64_t access_denied;
    float avg_duration_ms;
    uint64_t total_input_bytes;
    uint64_t total_output_bytes;
    uint64_t last_invoked_ms;
} rcog_tool_stats_t;

/**
 * @brief Router-wide statistics
 */
typedef struct {
    uint64_t total_invocations;
    uint64_t total_successes;
    uint64_t total_failures;
    uint64_t total_access_denied;
    uint64_t total_timeouts;
    uint32_t tools_registered;
    uint32_t categories_registered;
    uint32_t current_concurrent;
    uint32_t peak_concurrent;
    float avg_duration_ms;
} rcog_router_stats_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Get default router configuration
 * @return Default configuration
 */
rcog_tool_router_config_t rcog_tool_router_default_config(void);

/**
 * @brief Create tool router with configuration
 * @param config Configuration (NULL for defaults)
 * @return Router handle or NULL on error
 */
rcog_tool_router_t* rcog_tool_router_create(
    const rcog_tool_router_config_t* config
);

/**
 * @brief Create router with default configuration
 * @return Router handle or NULL on error
 */
rcog_tool_router_t* rcog_tool_router_create_default(void);

/**
 * @brief Destroy router and free resources
 * @param router Router handle (NULL safe)
 */
void rcog_tool_router_destroy(rcog_tool_router_t* router);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect to context store for input/output
 * @param router Router handle
 * @param store Context store handle
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_connect_context_store(
    rcog_tool_router_t* router,
    struct rcog_context_store* store
);

/**
 * @brief Connect to bio-async bridge for neuromodulation
 * @param router Router handle
 * @param bio_async Bio-async bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_connect_bio_async(
    rcog_tool_router_t* router,
    struct rcog_bio_async_bridge* bio_async
);

/**
 * @brief Connect to immune bridge for health checks
 * @param router Router handle
 * @param immune Immune bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_connect_immune(
    rcog_tool_router_t* router,
    struct rcog_immune_bridge* immune
);

/*=============================================================================
 * TOOL REGISTRATION
 *===========================================================================*/

/**
 * @brief Register a tool
 * @param router Router handle
 * @param def Tool definition
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_register(
    rcog_tool_router_t* router,
    const rcog_tool_def_t* def
);

/**
 * @brief Register multiple tools
 * @param router Router handle
 * @param defs Array of tool definitions
 * @param num_tools Number of tools
 * @return Number of successfully registered tools
 */
size_t rcog_tool_router_register_batch(
    rcog_tool_router_t* router,
    const rcog_tool_def_t* defs,
    size_t num_tools
);

/**
 * @brief Unregister a tool
 * @param router Router handle
 * @param tool_name Tool name
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_unregister(
    rcog_tool_router_t* router,
    const char* tool_name
);

/**
 * @brief Check if tool is registered
 * @param router Router handle
 * @param tool_name Tool name
 * @return true if registered
 */
bool rcog_tool_router_has_tool(
    const rcog_tool_router_t* router,
    const char* tool_name
);

/**
 * @brief Get tool definition
 * @param router Router handle
 * @param tool_name Tool name
 * @param def Output tool definition
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_get_tool(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_tool_def_t* def
);

/*=============================================================================
 * CATEGORY MANAGEMENT
 *===========================================================================*/

/**
 * @brief Register a tool category
 * @param router Router handle
 * @param category Category name
 * @param description Category description
 * @param default_tier Default tier for category
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_register_category(
    rcog_tool_router_t* router,
    const char* category,
    const char* description,
    rcog_capability_tier_t default_tier
);

/**
 * @brief Get tools in category
 * @param router Router handle
 * @param category Category name
 * @param tools Output array of tool names
 * @param max_tools Maximum tools to retrieve
 * @param num_tools Output number of tools
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_get_category_tools(
    const rcog_tool_router_t* router,
    const char* category,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
);

/**
 * @brief List all categories
 * @param router Router handle
 * @param categories Output array of category names
 * @param max_categories Maximum categories
 * @param num_categories Output number of categories
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_list_categories(
    const rcog_tool_router_t* router,
    char (*categories)[RCOG_ROUTER_MAX_CATEGORY_NAME],
    size_t max_categories,
    size_t* num_categories
);

/*=============================================================================
 * TOOL INVOCATION
 *===========================================================================*/

/**
 * @brief Invoke a tool synchronously
 * @param router Router handle
 * @param request Invocation request
 * @param result Output result (caller owns output data)
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_invoke(
    rcog_tool_router_t* router,
    const rcog_tool_request_t* request,
    rcog_tool_result_t* result
);

/**
 * @brief Invoke a tool asynchronously
 * @param router Router handle
 * @param request Invocation request
 * @param callback Completion callback
 * @param handle Output async handle (for cancellation)
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_invoke_async(
    rcog_tool_router_t* router,
    const rcog_tool_request_t* request,
    rcog_tool_callback_t callback,
    rcog_async_handle_t** handle
);

/**
 * @brief Invoke a tool with streaming output
 * @param router Router handle
 * @param request Invocation request
 * @param stream_callback Streaming callback
 * @param user_data Callback user data
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_invoke_streaming(
    rcog_tool_router_t* router,
    const rcog_tool_request_t* request,
    rcog_tool_stream_callback_t stream_callback,
    void* user_data
);

/**
 * @brief Cancel async tool invocation
 * @param router Router handle
 * @param handle Async handle
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_cancel(
    rcog_tool_router_t* router,
    rcog_async_handle_t* handle
);

/**
 * @brief Free tool result (output data)
 * @param result Result to free
 */
void rcog_tool_router_free_result(rcog_tool_result_t* result);

/*=============================================================================
 * ACCESS CONTROL
 *===========================================================================*/

/**
 * @brief Check if tier can access tool
 * @param router Router handle
 * @param tool_name Tool name
 * @param tier Caller tier
 * @return true if access allowed
 */
bool rcog_tool_router_can_access(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_capability_tier_t tier
);

/**
 * @brief Get tools accessible by tier
 * @param router Router handle
 * @param tier Capability tier
 * @param tools Output array of tool names
 * @param max_tools Maximum tools
 * @param num_tools Output number of tools
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_get_accessible_tools(
    const rcog_tool_router_t* router,
    rcog_capability_tier_t tier,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
);

/**
 * @brief Get minimum tier for tool
 * @param router Router handle
 * @param tool_name Tool name
 * @param tier Output minimum tier
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_get_min_tier(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_capability_tier_t* tier
);

/**
 * @brief Set access policy
 * @param router Router handle
 * @param policy New policy
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_set_access_policy(
    rcog_tool_router_t* router,
    const rcog_access_policy_t* policy
);

/*=============================================================================
 * DISCOVERY
 *===========================================================================*/

/**
 * @brief List all registered tools
 * @param router Router handle
 * @param tools Output array of tool names
 * @param max_tools Maximum tools
 * @param num_tools Output number of tools
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_list_tools(
    const rcog_tool_router_t* router,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
);

/**
 * @brief List tools by tier
 * @param router Router handle
 * @param tier Capability tier
 * @param tools Output array of tool names
 * @param max_tools Maximum tools
 * @param num_tools Output number of tools
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_list_tools_by_tier(
    const rcog_tool_router_t* router,
    rcog_capability_tier_t tier,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
);

/**
 * @brief Search tools by name pattern
 * @param router Router handle
 * @param pattern Search pattern (supports * wildcard)
 * @param tools Output array of tool names
 * @param max_tools Maximum tools
 * @param num_tools Output number of tools
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_search_tools(
    const rcog_tool_router_t* router,
    const char* pattern,
    char (*tools)[RCOG_ROUTER_MAX_TOOL_NAME],
    size_t max_tools,
    size_t* num_tools
);

/**
 * @brief Get tool count
 * @param router Router handle
 * @return Number of registered tools
 */
size_t rcog_tool_router_get_tool_count(
    const rcog_tool_router_t* router
);

/*=============================================================================
 * BUILTIN TOOLS
 *===========================================================================*/

/**
 * @brief Register L1 reasoning tools
 *
 * Registers built-in reasoning tools:
 * - memory_read: Read from context store
 * - memory_write: Write to context store
 * - memory_query: Query context with pattern
 * - logic_infer: Logical inference
 * - plan_create: Create action plan
 *
 * @param router Router handle
 * @return Number of tools registered
 */
size_t rcog_tool_router_register_l1_builtins(rcog_tool_router_t* router);

/**
 * @brief Register L2 perception tools
 *
 * Registers built-in perception tools:
 * - feature_extract: Extract features from input
 * - pattern_match: Match patterns in data
 * - similarity_compute: Compute similarity scores
 *
 * @param router Router handle
 * @return Number of tools registered
 */
size_t rcog_tool_router_register_l2_builtins(rcog_tool_router_t* router);

/**
 * @brief Register L3 action tools
 *
 * Registers built-in action tools:
 * - output_text: Generate text output
 * - output_structured: Generate structured output
 * - action_execute: Execute planned action
 *
 * @param router Router handle
 * @return Number of tools registered
 */
size_t rcog_tool_router_register_l3_builtins(rcog_tool_router_t* router);

/**
 * @brief Register all builtin tools
 * @param router Router handle
 * @return Total tools registered
 */
size_t rcog_tool_router_register_all_builtins(rcog_tool_router_t* router);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get router statistics
 * @param router Router handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_get_stats(
    const rcog_tool_router_t* router,
    rcog_router_stats_t* stats
);

/**
 * @brief Get per-tool statistics
 * @param router Router handle
 * @param tool_name Tool name
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_tool_router_get_tool_stats(
    const rcog_tool_router_t* router,
    const char* tool_name,
    rcog_tool_stats_t* stats
);

/**
 * @brief Reset all statistics
 * @param router Router handle
 */
void rcog_tool_router_reset_stats(rcog_tool_router_t* router);

/*=============================================================================
 * UTILITY
 *===========================================================================*/

/**
 * @brief Create default tool definition
 * @param name Tool name
 * @param handler Execution handler
 * @param tier Minimum tier
 * @return Tool definition with defaults
 */
rcog_tool_def_t rcog_tool_def_create(
    const char* name,
    rcog_tool_fn handler,
    rcog_capability_tier_t tier
);

/**
 * @brief Create tool request
 * @param tool_name Tool name
 * @param input Input data
 * @param input_size Input size
 * @param tier Caller tier
 * @return Tool request
 */
rcog_tool_request_t rcog_tool_request_create(
    const char* tool_name,
    const void* input,
    size_t input_size,
    rcog_capability_tier_t tier
);

/**
 * @brief Get tier name as string
 * @param tier Capability tier
 * @return Tier name
 */
const char* rcog_tool_tier_name(rcog_capability_tier_t tier);

/**
 * @brief Get IO type name as string
 * @param type IO type
 * @return Type name
 */
const char* rcog_tool_io_type_name(rcog_tool_io_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_TOOL_ROUTER_H */
