/**
 * @file nimcp_cognitive_event_types.h
 * @brief Cognitive event type definitions for cross-module communication
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Type definitions for cognitive events used throughout the integration hub
 * WHY:  Standardize event communication between cognitive modules
 * HOW:  Define enums, structs, and callback types for event-driven architecture
 *
 * DESIGN PATTERNS:
 * - Observer: Event callbacks for inter-module communication
 * - Strategy: Different event types trigger different processing strategies
 * - Command: Events encapsulate operations and their data
 *
 * THREAD SAFETY: All types are designed for multi-threaded environments.
 * Event data should be copied before async processing.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_COGNITIVE_EVENT_TYPES_H
#define NIMCP_COGNITIVE_EVENT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * COGNITIVE CATEGORY DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Categories of cognitive processing modules
 * WHY: Classify modules for routing and filtering events
 * HOW: Enum with distinct cognitive domain categories
 *
 * CATEGORIES:
 * - PERCEPTION: Sensory processing (visual, auditory, etc.)
 * - MEMORY: Memory systems (working, episodic, semantic)
 * - REASONING: Logic, inference, problem-solving
 * - EXECUTIVE: Planning, decision-making, control
 * - SOCIAL: Social cognition, theory of mind
 * - EMOTIONAL: Emotion processing, affect regulation
 * - SELF: Self-awareness, metacognition, introspection
 */
typedef enum {
    COG_CATEGORY_PERCEPTION = 0,  /* Sensory and perceptual processing */
    COG_CATEGORY_MEMORY,          /* Memory systems */
    COG_CATEGORY_REASONING,       /* Logic and inference */
    COG_CATEGORY_EXECUTIVE,       /* Executive control and planning */
    COG_CATEGORY_SOCIAL,          /* Social cognition */
    COG_CATEGORY_EMOTIONAL,       /* Emotion processing */
    COG_CATEGORY_SELF,            /* Self-awareness and metacognition */
    COG_CATEGORY_COUNT            /* Number of categories (sentinel) */
} cognitive_category_t;

/* ========================================================================
 * COGNITIVE EVENT TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Types of events that can occur in cognitive processing
 * WHY: Distinguish different event semantics for proper handling
 * HOW: Enum with comprehensive event type coverage
 *
 * EVENT TYPES:
 * - STATE_CHANGE: Module state transition
 * - INPUT_RECEIVED: New input available for processing
 * - OUTPUT_READY: Processing complete, output available
 * - ATTENTION_SHIFT: Attention focus changed
 * - MEMORY_ACCESS: Memory read/write operation
 * - EMOTION_UPDATE: Emotional state changed
 * - DECISION_MADE: Decision or choice completed
 * - SOCIAL_SIGNAL: Social information detected
 * - LEARNING_COMPLETE: Learning episode finished
 * - CONSOLIDATION: Memory consolidation event
 */
typedef enum {
    COG_EVENT_STATE_CHANGE = 0,   /* Module state changed */
    COG_EVENT_INPUT_RECEIVED,     /* New input received */
    COG_EVENT_OUTPUT_READY,       /* Output ready for consumption */
    COG_EVENT_ATTENTION_SHIFT,    /* Attention focus changed */
    COG_EVENT_MEMORY_ACCESS,      /* Memory accessed */
    COG_EVENT_EMOTION_UPDATE,     /* Emotional state updated */
    COG_EVENT_DECISION_MADE,      /* Decision completed */
    COG_EVENT_SOCIAL_SIGNAL,      /* Social signal detected */
    COG_EVENT_LEARNING_COMPLETE,  /* Learning episode completed */
    COG_EVENT_CONSOLIDATION,      /* Memory consolidation occurred */
    COG_EVENT_COUNT               /* Number of event types (sentinel) */
} cognitive_event_type_t;

/* ========================================================================
 * COGNITIVE QUERY TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Types of queries modules can make to each other
 * WHY: Enable structured inter-module information requests
 * HOW: Enum covering common query patterns
 *
 * QUERY TYPES:
 * - STATUS: Get module operational status
 * - STATE: Get module internal state
 * - CAPABILITY: Query module capabilities
 * - MEMORY: Request memory contents
 * - PREDICTION: Request predictive output
 * - ATTENTION: Query attention state
 * - EMOTION: Query emotional state
 * - METRICS: Request performance metrics
 */
typedef enum {
    COG_QUERY_STATUS = 0,         /* Module operational status */
    COG_QUERY_STATE,              /* Module internal state */
    COG_QUERY_CAPABILITY,         /* Module capabilities */
    COG_QUERY_MEMORY,             /* Memory contents request */
    COG_QUERY_PREDICTION,         /* Predictive output request */
    COG_QUERY_ATTENTION,          /* Attention state query */
    COG_QUERY_EMOTION,            /* Emotional state query */
    COG_QUERY_METRICS,            /* Performance metrics request */
    COG_QUERY_COUNT               /* Number of query types (sentinel) */
} cognitive_query_type_t;

/* ========================================================================
 * EVENT PRIORITY DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Priority levels for cognitive events
 * WHY: Enable prioritized event processing
 * HOW: Enum from lowest to highest priority
 */
typedef enum {
    COG_PRIORITY_LOW = 0,         /* Background, non-urgent */
    COG_PRIORITY_NORMAL,          /* Standard priority */
    COG_PRIORITY_HIGH,            /* Elevated priority */
    COG_PRIORITY_CRITICAL         /* Immediate processing required */
} cognitive_event_priority_t;

/* ========================================================================
 * EVENT DATA STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Container for cognitive event data
 * WHY: Standardize event information passing between modules
 * HOW: Struct with type, source, timestamp, and payload
 *
 * FIELDS:
 * - event_type: Type of event (cognitive_event_type_t)
 * - source_module_id: ID of module that generated event
 * - timestamp: Event generation time (microseconds since epoch)
 * - priority: Event priority level
 * - payload: Pointer to event-specific data
 * - payload_size: Size of payload in bytes
 *
 * MEMORY: Payload ownership depends on context:
 * - Synchronous: Caller owns payload
 * - Asynchronous: Hub copies payload, caller can free immediately
 */
typedef struct {
    cognitive_event_type_t event_type;  /* Type of event */
    uint32_t source_module_id;          /* Source module identifier */
    uint64_t timestamp;                 /* Event timestamp (microseconds) */
    cognitive_event_priority_t priority; /* Event priority */
    void* payload;                      /* Event-specific data */
    size_t payload_size;                /* Size of payload in bytes */
} cognitive_event_data_t;

/* ========================================================================
 * CALLBACK TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Callback function type for event notifications
 * WHY: Enable modules to receive asynchronous event notifications
 * HOW: Function pointer with event data and user context
 *
 * PARAMETERS:
 * - event: The event data (read-only, do not modify)
 * - user_data: User-provided context from subscription
 *
 * RETURN: 0 on success, -1 on error
 *
 * THREAD SAFETY: Callbacks may be invoked from any thread.
 * Implementations must be thread-safe.
 *
 * BLOCKING: Callbacks should not block. Long operations should
 * be queued for later processing.
 */
typedef int (*cognitive_event_callback_t)(const cognitive_event_data_t* event,
                                          void* user_data);

/* ========================================================================
 * QUERY DATA STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Container for module query requests
 * WHY: Standardize inter-module query format
 * HOW: Struct with query type and parameters
 */
typedef struct {
    cognitive_query_type_t query_type;  /* Type of query */
    void* query_params;                 /* Query-specific parameters */
    size_t params_size;                 /* Size of parameters */
} cognitive_query_t;

/**
 * WHAT: Container for query results
 * WHY: Standardize query response format
 * HOW: Struct with result data and status
 */
typedef struct {
    int status;                         /* 0 = success, -1 = error */
    void* result_data;                  /* Query result data */
    size_t result_size;                 /* Size of result data */
    char error_message[128];            /* Error message if status != 0 */
} cognitive_query_result_t;

/* ========================================================================
 * UTILITY MACROS
 * ======================================================================== */

/**
 * WHAT: Helper macro to check if category is valid
 */
#define COG_CATEGORY_IS_VALID(cat) \
    ((cat) >= COG_CATEGORY_PERCEPTION && (cat) < COG_CATEGORY_COUNT)

/**
 * WHAT: Helper macro to check if event type is valid
 */
#define COG_EVENT_TYPE_IS_VALID(type) \
    ((type) >= COG_EVENT_STATE_CHANGE && (type) < COG_EVENT_COUNT)

/**
 * WHAT: Helper macro to check if query type is valid
 */
#define COG_QUERY_TYPE_IS_VALID(type) \
    ((type) >= COG_QUERY_STATUS && (type) < COG_QUERY_COUNT)

/* ========================================================================
 * STRING CONVERSION UTILITIES
 * ======================================================================== */

/**
 * WHAT: Get string name for cognitive category
 * WHY: Debug output and logging
 * HOW: Return static string for category
 *
 * @param category Cognitive category
 * @return String name or "UNKNOWN" if invalid
 */
const char* cognitive_category_to_string(cognitive_category_t category);

/**
 * WHAT: Get string name for event type
 * WHY: Debug output and logging
 * HOW: Return static string for event type
 *
 * @param event_type Event type
 * @return String name or "UNKNOWN" if invalid
 */
const char* cognitive_event_type_to_string(cognitive_event_type_t event_type);

/**
 * WHAT: Get string name for query type
 * WHY: Debug output and logging
 * HOW: Return static string for query type
 *
 * @param query_type Query type
 * @return String name or "UNKNOWN" if invalid
 */
const char* cognitive_query_type_to_string(cognitive_query_type_t query_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COGNITIVE_EVENT_TYPES_H */
