/**
 * @file nimcp_claude_healer.h
 * @brief Claude API Fallback Healer - External AI-Assisted Code Repair
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Fallback healer that uses Claude API when pattern matching fails
 * WHY:  Handle novel crashes that don't match known patterns by leveraging
 *       Claude's code understanding and repair capabilities
 * HOW:  Send crash context to Claude Opus API, receive and parse code fixes
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
 * -------------------------------------------------------------------
 * External Specialist Consult  -> Claude API call for complex cases
 * Adaptive Immune Response     -> Learning from Claude-suggested fixes
 * Cytokine Communication       -> Request/response protocol with API
 * B Cell Affinity Maturation   -> Improving prompts based on success
 * Immunological Memory         -> Caching successful Claude fixes
 * ```
 *
 * ARCHITECTURE:
 * ```
 * +-------------------------------------------------------------------+
 * |                      CLAUDE HEALER                                 |
 * +-------------------------------------------------------------------+
 * |                                                                    |
 * |  +----------------+    +------------------+    +----------------+  |
 * |  |  Crash Context |    |  Prompt          |    |  API Request   |  |
 * |  |  Collector     |--->|  Formatter       |--->|  Handler       |  |
 * |  +----------------+    +------------------+    +----------------+  |
 * |                                                        |           |
 * |                                                        v           |
 * |  +----------------+    +------------------+    +----------------+  |
 * |  |  Fix           |<---|  Response        |<---|  HTTPS/libcurl |  |
 * |  |  Validator     |    |  Parser          |    |  Client        |  |
 * |  +----------------+    +------------------+    +----------------+  |
 * |                                                                    |
 * +-------------------------------------------------------------------+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CLAUDE_HEALER_H
#define NIMCP_CLAUDE_HEALER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CLAUDE_HEALER_MAX_SOURCE_SIZE      8192   /**< Max source code context */
#define CLAUDE_HEALER_MAX_BACKTRACE_SIZE   4096   /**< Max backtrace string */
#define CLAUDE_HEALER_MAX_FIX_SIZE         8192   /**< Max fix code size */
#define CLAUDE_HEALER_MAX_EXPLANATION_SIZE 2048   /**< Max explanation size */
#define CLAUDE_HEALER_MAX_ERROR_SIZE       512    /**< Max error message size */
#define CLAUDE_HEALER_MAX_FAILED_FIXES     4096   /**< Max failed fixes history */
#define CLAUDE_HEALER_MAX_INCLUDES_SIZE    2048   /**< Max includes context */
#define CLAUDE_HEALER_MAX_SIGNATURE_SIZE   512    /**< Max function signature */
#define CLAUDE_HEALER_MAX_VARIABLES_SIZE   2048   /**< Max local variables */
#define CLAUDE_HEALER_MAX_API_KEY_SIZE     256    /**< Max API key length */
#define CLAUDE_HEALER_MAX_MODEL_SIZE       64     /**< Max model name length */

#define CLAUDE_HEALER_DEFAULT_TIMEOUT_MS   30000  /**< Default 30s timeout */
#define CLAUDE_HEALER_DEFAULT_MAX_RPM      10     /**< Default max requests/minute */
#define CLAUDE_HEALER_DEFAULT_MAX_RETRIES  3      /**< Default max retries */
#define CLAUDE_HEALER_BACKOFF_BASE_MS      1000   /**< Base backoff time 1s */
#define CLAUDE_HEALER_BACKOFF_MAX_MS       16000  /**< Max backoff time 16s */

#define CLAUDE_HEALER_MODULE_NAME          "claude_healer"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct claude_healer_s claude_healer_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Claude healer status codes
 *
 * WHAT: Result codes for Claude API operations
 * WHY:  Distinguish between different failure modes
 */
typedef enum {
    CLAUDE_HEAL_SUCCESS = 0,           /**< Fix generated successfully */
    CLAUDE_HEAL_ERROR_DISABLED,        /**< Claude API disabled (no libcurl) */
    CLAUDE_HEAL_ERROR_RATE_LIMITED,    /**< Rate limit exceeded */
    CLAUDE_HEAL_ERROR_TIMEOUT,         /**< Request timed out */
    CLAUDE_HEAL_ERROR_NETWORK,         /**< Network error */
    CLAUDE_HEAL_ERROR_API,             /**< API returned error */
    CLAUDE_HEAL_ERROR_PARSE,           /**< Failed to parse response */
    CLAUDE_HEAL_ERROR_INVALID_INPUT,   /**< Invalid input parameters */
    CLAUDE_HEAL_ERROR_NO_FIX,          /**< Claude couldn't generate fix */
    CLAUDE_HEAL_ERROR_LOW_CONFIDENCE,  /**< Confidence below threshold */
    CLAUDE_HEAL_ERROR_MEMORY,          /**< Memory allocation failed */
    CLAUDE_HEAL_ERROR_INTERNAL         /**< Internal error */
} claude_heal_status_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Request structure for Claude healing
 *
 * WHAT: Complete crash context for Claude to analyze
 * WHY:  Provide all necessary information for accurate fix generation
 * HOW:  Collected from crash handler, DWARF info, and previous attempts
 */
typedef struct {
    /* Crash information */
    int signal;                                    /**< Signal number (SIGSEGV, etc.) */
    const char* signal_name;                       /**< Signal name string */
    void* fault_address;                           /**< Address that caused fault */

    /* Source context */
    const char* source_file;                       /**< Source file path */
    uint32_t line_number;                          /**< Line number of crash */
    const char* function_name;                     /**< Function that crashed */
    const char* source_code;                       /**< ~50 lines around crash */

    /* Stack trace */
    const char* backtrace;                         /**< Formatted backtrace string */

    /* Memory state (if available from DWARF) */
    const char* local_variables;                   /**< Local variable values */

    /* Previous attempts */
    const char* failed_fixes;                      /**< What fixes already tried */

    /* Project context */
    const char* file_includes;                     /**< Header dependencies */
    const char* function_signature;                /**< Full function prototype */
} claude_heal_request_t;

/**
 * @brief Response structure from Claude healing
 *
 * WHAT: Claude's suggested fix and metadata
 * WHY:  Provide fix code and context for application
 */
typedef struct {
    char* fixed_code;                              /**< Generated fix code */
    size_t fixed_code_len;                         /**< Length of fixed code */
    char* explanation;                             /**< Why this fix works */
    float confidence;                              /**< 0-1 confidence score */
    bool success;                                  /**< Whether fix was generated */
    char* error_message;                           /**< Error message if failed */
} claude_heal_response_t;

/**
 * @brief Claude healer configuration
 *
 * WHAT: Settings for Claude API interaction
 * WHY:  Control API behavior, rate limits, timeouts
 */
typedef struct {
    char api_key[CLAUDE_HEALER_MAX_API_KEY_SIZE];  /**< Anthropic API key */
    char model[CLAUDE_HEALER_MAX_MODEL_SIZE];      /**< Model to use (claude-3-opus-*) */

    /* Timeouts and retries */
    uint32_t timeout_ms;                           /**< Request timeout */
    uint32_t max_retries;                          /**< Max retry attempts */

    /* Rate limiting */
    uint32_t max_requests_per_minute;              /**< Rate limit */

    /* Behavior */
    float min_confidence;                          /**< Min confidence threshold */
    bool enable_logging;                           /**< Enable debug logging */
    bool dry_run;                                  /**< Don't actually call API */
} claude_healer_config_t;

/**
 * @brief Claude healer statistics
 *
 * WHAT: Runtime metrics for monitoring
 * WHY:  Track API usage, success rates, costs
 */
typedef struct {
    uint64_t requests_made;                        /**< Total API requests */
    uint64_t requests_succeeded;                   /**< Successful requests */
    uint64_t requests_failed;                      /**< Failed requests */
    uint64_t requests_rate_limited;                /**< Rate limited requests */
    uint64_t requests_timed_out;                   /**< Timed out requests */
    uint64_t fixes_generated;                      /**< Fixes generated */
    uint64_t fixes_applied;                        /**< Fixes successfully applied */
    float avg_confidence;                          /**< Average fix confidence */
    double avg_response_time_ms;                   /**< Average response time */
    uint64_t total_tokens_used;                    /**< Estimated tokens used */
} claude_healer_stats_t;

/**
 * @brief Claude healer state (opaque)
 *
 * WHAT: Internal healer state
 * WHY:  Encapsulate implementation details
 */
struct claude_healer_s {
    claude_healer_config_t config;                 /**< Configuration */

    /* Rate limiting state */
    uint64_t* request_timestamps;                  /**< Ring buffer of timestamps */
    size_t request_count;                          /**< Requests in current window */
    size_t timestamp_capacity;                     /**< Timestamp buffer size */
    size_t timestamp_head;                         /**< Ring buffer head */

    /* Statistics */
    claude_healer_stats_t stats;                   /**< Runtime statistics */

    /* Thread safety */
    void* mutex;                                   /**< Access mutex */

    /* State */
    bool initialized;                              /**< Healer initialized */
    bool api_available;                            /**< libcurl available */

#ifdef HAVE_LIBCURL
    void* curl;                                    /**< CURL handle (reused) */
#endif
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Easy initialization
 * HOW:  Set default model, timeout, rate limits
 *
 * @param config Configuration to populate
 * @return 0 on success, -1 on error
 */
int claude_healer_default_config(claude_healer_config_t* config);

/**
 * @brief Create Claude healer
 *
 * WHAT: Initialize Claude healer with given configuration
 * WHY:  Set up API client, rate limiter, and state
 * HOW:  Validate config, init libcurl if available, allocate buffers
 *
 * @param config Configuration (NULL for defaults - requires API key env var)
 * @return Healer handle or NULL on failure
 */
claude_healer_t* claude_healer_create(const claude_healer_config_t* config);

/**
 * @brief Destroy Claude healer
 *
 * WHAT: Clean up healer resources
 * WHY:  Proper resource deallocation
 * HOW:  Free buffers, close curl handle
 *
 * @param healer Healer to destroy (NULL-safe)
 */
void claude_healer_destroy(claude_healer_t* healer);

/* ============================================================================
 * Core Healing API
 * ============================================================================ */

/**
 * @brief Request fix from Claude API
 *
 * WHAT: Send crash context to Claude and receive fix
 * WHY:  Main entry point for Claude-assisted healing
 * HOW:  Format prompt, make API call, parse response
 *
 * @param healer Claude healer instance
 * @param request Crash context and request info
 * @param response Output: Claude's response with fix
 * @return Status code (CLAUDE_HEAL_SUCCESS on success)
 */
claude_heal_status_t claude_healer_request_fix(
    claude_healer_t* healer,
    const claude_heal_request_t* request,
    claude_heal_response_t* response
);

/**
 * @brief Format crash context into prompt
 *
 * WHAT: Create structured prompt from crash context
 * WHY:  Consistent, effective prompting for Claude
 * HOW:  Template expansion with crash details
 *
 * @param request Crash context
 * @param prompt_out Output buffer for formatted prompt
 * @param prompt_size Size of output buffer
 * @return Length of formatted prompt, or -1 on error
 */
int claude_healer_format_prompt(
    const claude_heal_request_t* request,
    char* prompt_out,
    size_t prompt_size
);

/**
 * @brief Free response resources
 *
 * WHAT: Free memory allocated in response
 * WHY:  Clean up after processing response
 * HOW:  Free fixed_code, explanation, error_message
 *
 * @param response Response to free (NULL-safe)
 */
void claude_healer_free_response(claude_heal_response_t* response);

/**
 * @brief Send raw prompt to Claude API
 *
 * WHAT: Send custom prompt and receive raw response
 * WHY:  Allow advanced/custom prompting for special cases
 * HOW:  Make API call with custom prompt, return raw text
 *
 * Note: This bypasses the standard prompt formatting and fix extraction.
 * Use claude_healer_request_fix for standard crash healing workflow.
 *
 * @param healer Claude healer instance
 * @param prompt Custom prompt string
 * @param response_out Output buffer for response text
 * @param response_size Size of output buffer
 * @param response_len Output: actual response length
 * @return Status code (CLAUDE_HEAL_SUCCESS on success)
 */
claude_heal_status_t claude_healer_send_request(
    claude_healer_t* healer,
    const char* prompt,
    char* response_out,
    size_t response_size,
    size_t* response_len
);

/**
 * @brief Validate C code syntax
 *
 * WHAT: Basic syntax validation for generated C code
 * WHY:  Ensure generated fixes are syntactically plausible
 * HOW:  Check balanced braces, parentheses, and strings
 *
 * @param code C code to validate
 * @param code_len Length of code string
 * @return true if code passes basic validation
 */
bool claude_healer_validate_code(const char* code, size_t code_len);

/* ============================================================================
 * Rate Limiting API
 * ============================================================================ */

/**
 * @brief Check if request is allowed by rate limiter
 *
 * WHAT: Check if we can make another API request
 * WHY:  Respect API rate limits
 * HOW:  Sliding window algorithm
 *
 * @param healer Claude healer instance
 * @return true if request allowed, false if rate limited
 */
bool claude_healer_check_rate_limit(claude_healer_t* healer);

/**
 * @brief Record a request for rate limiting
 *
 * WHAT: Record that a request was made
 * WHY:  Track requests for rate limiting
 * HOW:  Add timestamp to ring buffer
 *
 * @param healer Claude healer instance
 */
void claude_healer_record_request(claude_healer_t* healer);

/**
 * @brief Get time until rate limit resets
 *
 * WHAT: Calculate wait time until next request allowed
 * WHY:  Allow caller to wait or schedule retry
 * HOW:  Check oldest request in window
 *
 * @param healer Claude healer instance
 * @return Milliseconds until rate limit resets (0 if not limited)
 */
uint64_t claude_healer_get_rate_limit_reset_ms(claude_healer_t* healer);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get healer statistics
 *
 * WHAT: Retrieve runtime statistics
 * WHY:  Monitor API usage and success rates
 * HOW:  Copy statistics structure
 *
 * @param healer Claude healer instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int claude_healer_get_stats(
    claude_healer_t* healer,
    claude_healer_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero statistics structure
 *
 * @param healer Claude healer instance
 * @return 0 on success, -1 on error
 */
int claude_healer_reset_stats(claude_healer_t* healer);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if Claude API is available
 *
 * WHAT: Check if libcurl is compiled in
 * WHY:  Graceful fallback when not available
 * HOW:  Check HAVE_LIBCURL define
 *
 * @return true if API is available
 */
bool claude_healer_is_available(void);

/**
 * @brief Get status string
 *
 * @param status Status code
 * @return String description
 */
const char* claude_healer_status_to_string(claude_heal_status_t status);

/**
 * @brief Get signal name from number
 *
 * @param signal Signal number
 * @return Signal name string
 */
const char* claude_healer_signal_name(int signal);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CLAUDE_HEALER_H */
