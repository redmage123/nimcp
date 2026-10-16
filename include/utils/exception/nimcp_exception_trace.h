/**
 * @file nimcp_exception_trace.h
 * @brief Distributed tracing and cross-module exception propagation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Exception correlation IDs and KG-aware exception routing
 * WHY:  Enable distributed tracing across swarm nodes and neural pathways
 * HOW:  Correlation IDs follow W3C trace-context standard; propagation
 *       paths track exception routing through KG modules
 *
 * ARCHITECTURE:
 * ```
 * Node A                    Node B                    Node C
 *   |                         |                         |
 *   +-- trace_id: 0xABC123 --+                         |
 *   |   span_id: 0x001       |                         |
 *   |                        +-- trace_id: 0xABC123 --+
 *   |                        |   span_id: 0x002       |
 *   |                        |   parent: 0x001        |
 *   |                        |                        +-- trace_id: 0xABC123
 *   |                        |                        |   span_id: 0x003
 *   |                        |                        |   parent: 0x002
 * ```
 *
 * PROPAGATION PATH EXAMPLE:
 * ```
 * Exception in Visual Cortex
 *   -> KG_MSG_ERROR to Wernicke Area (priority 8)
 *   -> KG_MSG_ALERT to Prefrontal Cortex (priority 9)
 *   -> KG_MSG_RECOVERY to Immune System (priority 10)
 * ```
 *
 * INTEGRATION:
 * - Trace IDs correlate exceptions across distributed swarm nodes
 * - Propagation paths enable KG-aware routing through brain modules
 * - W3C trace-context format for external system integration
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXCEPTION_TRACE_H
#define NIMCP_EXCEPTION_TRACE_H

#include "utils/exception/nimcp_exception.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_TRACE_ID_SIZE         16    /**< Size of trace ID in bytes */
#define NIMCP_MAX_PROPAGATION_PATH  32    /**< Max hops in propagation path */
#define NIMCP_MAX_MODULE_NAME_LEN   64    /**< Max length of module name */
#define NIMCP_MAX_MSG_TYPE_LEN      32    /**< Max length of message type */
#define NIMCP_TRACE_HEADER_SIZE     128   /**< Max size of W3C header string */
#define NIMCP_MAX_TRACE_STACK       16    /**< Max depth of trace context stack */

/* Trace flags */
#define NIMCP_TRACE_FLAG_SAMPLED    0x01  /**< Trace is being sampled */
#define NIMCP_TRACE_FLAG_DEBUG      0x02  /**< Debug mode enabled */
#define NIMCP_TRACE_FLAG_DEFERRED   0x04  /**< Deferred sampling decision */
#define NIMCP_TRACE_FLAG_SECURE     0x08  /**< Secure/encrypted trace */

/* ============================================================================
 * Distributed Trace Context
 * ============================================================================ */

/**
 * @brief Distributed trace context for correlation across nodes
 *
 * Follows W3C trace-context specification for interoperability.
 * Each span represents a unit of work within the trace.
 */
typedef struct {
    uint64_t trace_id;           /**< Unique trace ID across system */
    uint64_t span_id;            /**< Current span within trace */
    uint64_t parent_span_id;     /**< Parent span (0 if root) */
    uint32_t node_id;            /**< Swarm node ID originating trace */
    uint64_t start_time_us;      /**< Span start time in microseconds */
    uint8_t trace_flags;         /**< Sampling, debug flags */
} nimcp_exception_trace_t;

/* ============================================================================
 * Cross-Module Propagation
 * ============================================================================ */

/**
 * @brief Single hop in the propagation path
 *
 * Tracks how an exception moves through KG modules.
 */
typedef struct {
    const char* module_name;     /**< KG module name (e.g., "visual_cortex") */
    const char* message_type;    /**< KG message type (e.g., "KG_MSG_ERROR") */
    uint64_t timestamp_us;       /**< When exception reached this module */
    uint32_t priority;           /**< Message priority (1-10) */
} nimcp_propagation_entry_t;

/**
 * @brief Cross-module propagation context
 *
 * Tracks the complete path an exception takes through the brain's
 * neural pathways (KG modules).
 */
typedef struct {
    nimcp_propagation_entry_t path[NIMCP_MAX_PROPAGATION_PATH];
    size_t path_length;          /**< Number of entries in path */
    const char* origin_module;   /**< Module where exception originated */
    const char* target_module;   /**< Target module for recovery */
    bool requires_coordination;  /**< Multi-module recovery needed */
} nimcp_propagation_context_t;

/* ============================================================================
 * Trace System API
 * ============================================================================ */

/**
 * @brief Initialize the trace system
 *
 * WHAT: Initialize trace ID generation and thread-local storage
 * WHY:  Required before creating traces
 * HOW:  Sets up atomic counters and node ID
 *
 * @return 0 on success, -1 on error
 */
int nimcp_trace_init(void);

/**
 * @brief Shutdown the trace system
 *
 * WHAT: Clean up trace system resources
 * WHY:  Release thread-local storage and counters
 */
void nimcp_trace_shutdown(void);

/**
 * @brief Create a new root trace
 *
 * WHAT: Create a trace with new trace_id and span_id
 * WHY:  Start a new distributed trace
 * HOW:  Atomically generates unique IDs
 *
 * @return New trace context (stack-allocated, copy as needed)
 */
nimcp_exception_trace_t nimcp_trace_create(void);

/**
 * @brief Create a child span within existing trace
 *
 * WHAT: Create a new span under a parent trace
 * WHY:  Continue trace through function calls / module boundaries
 * HOW:  Copies trace_id, generates new span_id, sets parent
 *
 * @param parent Parent trace context
 * @return New child trace context
 */
nimcp_exception_trace_t nimcp_trace_create_child(const nimcp_exception_trace_t* parent);

/**
 * @brief Attach trace context to exception
 *
 * WHAT: Associate distributed trace with exception
 * WHY:  Enable trace correlation when exception is logged/propagated
 *
 * @param ex Exception to modify
 * @param trace Trace context to attach
 * @return 0 on success, -1 on error
 */
int nimcp_exception_set_trace(nimcp_exception_t* ex, const nimcp_exception_trace_t* trace);

/**
 * @brief Get trace context from exception
 *
 * WHAT: Retrieve attached trace context
 * WHY:  Access trace info for logging, propagation
 *
 * @param ex Exception to query
 * @return Trace context or NULL if not set
 */
const nimcp_exception_trace_t* nimcp_exception_get_trace(const nimcp_exception_t* ex);

/**
 * @brief Generate a unique trace/span ID
 *
 * WHAT: Atomically generate unique 64-bit ID
 * WHY:  Thread-safe ID generation for traces
 * HOW:  Combines node_id, timestamp, and atomic counter
 *
 * @return Unique 64-bit ID
 */
uint64_t nimcp_trace_generate_id(void);

/**
 * @brief Set the node ID for this process
 *
 * WHAT: Set swarm node identifier
 * WHY:  Node ID is embedded in trace IDs for distributed tracing
 *
 * @param node_id Unique node identifier in swarm
 */
void nimcp_trace_set_node_id(uint32_t node_id);

/**
 * @brief Get the current node ID
 *
 * @return Current node ID
 */
uint32_t nimcp_trace_get_node_id(void);

/* ============================================================================
 * Thread-Local Trace Context Stack
 * ============================================================================ */

/**
 * @brief Push trace context onto thread-local stack
 *
 * WHAT: Save current trace context for nested operations
 * WHY:  Maintain trace hierarchy through nested calls
 *
 * @param trace Trace to push
 * @return 0 on success, -1 if stack full
 */
int nimcp_trace_push(const nimcp_exception_trace_t* trace);

/**
 * @brief Get current trace context from thread-local stack
 *
 * WHAT: Retrieve the current (top) trace context
 * WHY:  Access active trace for exception correlation
 *
 * @return Current trace or NULL if stack empty
 */
nimcp_exception_trace_t* nimcp_trace_current(void);

/**
 * @brief Pop trace context from thread-local stack
 *
 * WHAT: Remove top trace context
 * WHY:  Restore previous trace when exiting scope
 */
void nimcp_trace_pop(void);

/* ============================================================================
 * Propagation API
 * ============================================================================ */

/**
 * @brief Create a propagation context
 *
 * WHAT: Allocate and initialize propagation tracking
 * WHY:  Track exception path through KG modules
 *
 * @param origin_module Module where exception originated
 * @return New propagation context or NULL on error
 */
nimcp_propagation_context_t* nimcp_propagation_create(const char* origin_module);

/**
 * @brief Destroy a propagation context
 *
 * WHAT: Free propagation context resources
 *
 * @param ctx Context to destroy
 */
void nimcp_propagation_destroy(nimcp_propagation_context_t* ctx);

/**
 * @brief Add a hop to the propagation path
 *
 * WHAT: Record exception passing through a module
 * WHY:  Track complete path for debugging and coordination
 *
 * @param ctx Propagation context
 * @param module Module name
 * @param msg_type KG message type used
 * @param priority Message priority
 * @return 0 on success, -1 if path full or error
 */
int nimcp_propagation_add_hop(
    nimcp_propagation_context_t* ctx,
    const char* module,
    const char* msg_type,
    uint32_t priority
);

/**
 * @brief Set the target module for recovery
 *
 * WHAT: Specify which module should handle recovery
 * WHY:  Route exception to appropriate handler
 *
 * @param ctx Propagation context
 * @param target Target module name
 * @return 0 on success, -1 on error
 */
int nimcp_propagation_set_target(
    nimcp_propagation_context_t* ctx,
    const char* target
);

/**
 * @brief Attach propagation context to exception
 *
 * WHAT: Associate propagation path with exception
 * WHY:  Enable KG-aware routing
 *
 * @param ex Exception to modify
 * @param ctx Propagation context (ownership transferred)
 * @return 0 on success, -1 on error
 */
int nimcp_exception_set_propagation(
    nimcp_exception_t* ex,
    nimcp_propagation_context_t* ctx
);

/**
 * @brief Get propagation context from exception
 *
 * WHAT: Retrieve attached propagation path
 * WHY:  Access path for routing decisions
 *
 * @param ex Exception to query
 * @return Propagation context or NULL if not set
 */
const nimcp_propagation_context_t* nimcp_exception_get_propagation(
    const nimcp_exception_t* ex
);

/**
 * @brief Check if exception requires multi-module coordination
 *
 * WHAT: Determine if recovery needs multiple modules
 * WHY:  Some exceptions affect multiple brain regions
 *
 * @param ctx Propagation context
 * @return true if coordination required
 */
bool nimcp_propagation_requires_coordination(
    const nimcp_propagation_context_t* ctx
);

/* ============================================================================
 * W3C Trace-Context Format
 * ============================================================================ */

/**
 * @brief Format trace as W3C traceparent header
 *
 * WHAT: Convert trace to "00-{trace_id}-{span_id}-{flags}" format
 * WHY:  Interoperability with external tracing systems
 * HOW:  Follows W3C trace-context specification
 *
 * Format: "00-{32 hex trace_id}-{16 hex span_id}-{2 hex flags}"
 * Example: "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"
 *
 * @param trace Trace context to format
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator)
 */
size_t nimcp_trace_to_header(
    const nimcp_exception_trace_t* trace,
    char* buffer,
    size_t size
);

/**
 * @brief Parse W3C traceparent header into trace context
 *
 * WHAT: Parse "00-{trace_id}-{span_id}-{flags}" format
 * WHY:  Accept traces from external systems
 *
 * @param header Header string to parse
 * @param trace Output trace context
 * @return 0 on success, -1 on parse error
 */
int nimcp_trace_from_header(
    const char* header,
    nimcp_exception_trace_t* trace
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Format trace context as human-readable string
 *
 * @param trace Trace context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
size_t nimcp_trace_to_string(
    const nimcp_exception_trace_t* trace,
    char* buffer,
    size_t size
);

/**
 * @brief Format propagation path as human-readable string
 *
 * @param ctx Propagation context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
size_t nimcp_propagation_to_string(
    const nimcp_propagation_context_t* ctx,
    char* buffer,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXCEPTION_TRACE_H */
