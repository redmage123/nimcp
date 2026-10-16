/**
 * @file nimcp_imagination_reasoning_bridge.h
 * @brief Imagination-Reasoning Cognitive Integration Hub Bridge
 * @version 1.1.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting imagination and reasoning modules through the
 *       cognitive integration hub for bidirectional event-driven communication.
 *
 * WHY: Imagination and reasoning must work together for:
 *      - Counterfactual reasoning: Analyzing "what-if" scenarios
 *      - Creative inference: Making novel logical leaps from imagined premises
 *      - Simulation-guided reasoning: Using mental simulation to inform logic
 *      - Scenario planning: Combining imaginative exploration with structured analysis
 *
 * HOW: Registers with the cognitive hub, subscribes to relevant events,
 *      and provides specialized operations for imagination-reasoning coordination.
 *
 * THEORETICAL BASIS:
 * - Default Mode Network (DMN): Supports imagination and self-projection
 * - Mental Simulation Theory: Imagination provides scenarios for reasoning to analyze
 * - Counterfactual Thinking: Reasoning about alternatives to actual events
 * - Creative Cognition: Novel combinations of imagination and logical inference
 * - Prefrontal Integration: Imagined scenarios combined with logical reasoning
 *
 * INTEGRATION PATTERNS:
 * - Subscribe: COG_EVENT_INPUT_RECEIVED for new reasoning problems
 * - Subscribe: COG_EVENT_ATTENTION_SHIFT for focus changes
 * - Subscribe: COG_EVENT_OUTPUT_READY for simulation results
 * - Subscribe: COG_EVENT_LEARNING_COMPLETE for creative inferences
 * - Subscribe: COG_EVENT_STATE_CHANGE for imagination state updates
 * - Publish: COG_EVENT_OUTPUT_READY when scenario analyzed
 * - Query: Expose scenario evaluation to other modules
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_IMAGINATION_REASONING_BRIDGE_H
#define NIMCP_IMAGINATION_REASONING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum concurrent scenarios */
#define IMAG_REASON_MAX_SCENARIOS           64

/** Default module ID for imagination-reasoning bridge */
#define IMAG_REASON_DEFAULT_MODULE_ID       0x494D5253  /* "IMRS" */

/** Maximum scenario complexity level */
#define IMAG_REASON_MAX_COMPLEXITY          10

/** Maximum scenario description length */
#define IMAG_REASON_MAX_SCENARIO_LEN        1024

/** Maximum premises for creative inference */
#define IMAG_REASON_MAX_PREMISES            16

/** Maximum insight description length */
#define IMAG_REASON_MAX_INSIGHT_LEN         512

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct imagination_reasoning_bridge imagination_reasoning_bridge_t;

/* Forward declare external types */
struct cognitive_integration_hub_struct;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;

/* Forward declare imagination and reasoning types */
struct imagination_engine;
typedef struct imagination_engine imagination_engine_t;
struct reasoning_engine;
typedef struct reasoning_engine reasoning_engine_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    IMAG_REASON_STATE_IDLE = 0,           /**< No active scenarios */
    IMAG_REASON_STATE_IMAGINING,          /**< Generating scenarios */
    IMAG_REASON_STATE_ANALYZING,          /**< Reasoning about scenarios */
    IMAG_REASON_STATE_INTEGRATING,        /**< Combining results */
    IMAG_REASON_STATE_ERROR               /**< Error state */
} imagination_reasoning_state_t;

/**
 * @brief Scenario type enumeration
 */
typedef enum {
    IMAG_SCENARIO_COUNTERFACTUAL = 0,     /**< What-if scenarios */
    IMAG_SCENARIO_PROSPECTIVE,            /**< Future projection */
    IMAG_SCENARIO_HYPOTHETICAL,           /**< Abstract hypothetical */
    IMAG_SCENARIO_EPISODIC,               /**< Memory-based simulation */
    IMAG_SCENARIO_COUNT
} imagination_scenario_type_t;

/**
 * @brief Types of imagination-reasoning bridge events
 *
 * WHAT: Enumeration of bridge-specific event subtypes
 * WHY: Allow subscribers to filter on specific bridge event types
 * HOW: Included in event payload for detailed routing
 */
typedef enum {
    /** Counterfactual analysis request from imagination to reasoning */
    IMAG_REASON_EVENT_COUNTERFACTUAL_REQUEST = 0,

    /** Counterfactual analysis result from reasoning */
    IMAG_REASON_EVENT_COUNTERFACTUAL_RESULT,

    /** Simulation result shared from imagination */
    IMAG_REASON_EVENT_SIMULATION_RESULT,

    /** Simulation acknowledged by reasoning */
    IMAG_REASON_EVENT_SIMULATION_PROCESSED,

    /** Creative inference request from imagination */
    IMAG_REASON_EVENT_CREATIVE_REQUEST,

    /** Creative inference result from reasoning */
    IMAG_REASON_EVENT_CREATIVE_RESULT,

    /** Imagination insight published */
    IMAG_REASON_EVENT_INSIGHT_PUBLISHED,

    /** Reasoning feedback on insight */
    IMAG_REASON_EVENT_INSIGHT_FEEDBACK,

    /** Imagination state updated */
    IMAG_REASON_EVENT_IMAGINATION_STATE,

    /** Reasoning state updated */
    IMAG_REASON_EVENT_REASONING_STATE,

    /** Count of event types */
    IMAG_REASON_EVENT_COUNT
} imag_reason_event_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Imagination-Reasoning bridge
 */
typedef struct {
    uint32_t module_id;                   /**< Module ID for hub registration */
    uint32_t max_concurrent_scenarios;    /**< Max scenarios to process */
    float imagination_weight;             /**< Weight of imagination input */
    float reasoning_weight;               /**< Weight of reasoning output */
    bool auto_subscribe_input;            /**< Subscribe to INPUT_RECEIVED */
    bool auto_subscribe_attention;        /**< Subscribe to ATTENTION_SHIFT */
    bool enable_counterfactual;           /**< Enable counterfactual reasoning */
    bool enable_prospective;              /**< Enable future projection */
    bool enable_logging;                  /**< Enable logging of bridge operations */
    float counterfactual_weight;          /**< Weight for counterfactual analysis (0-1) */
    float simulation_reasoning_weight;    /**< Weight for simulation-guided reasoning (0-1) */
    float creative_inference_weight;      /**< Weight for creative inference (0-1) */
    bool enable_query_handling;           /**< Enable query handling */
    uint32_t event_priority;              /**< Priority for published events */
} imagination_reasoning_config_t;

/**
 * @brief Scenario for imagination-reasoning integration
 */
typedef struct {
    uint64_t scenario_id;                 /**< Unique scenario identifier */
    imagination_scenario_type_t type;     /**< Type of scenario */
    float plausibility;                   /**< Scenario plausibility [0,1] */
    float relevance;                      /**< Relevance to current goal [0,1] */
    float complexity;                     /**< Scenario complexity [0,1] */
    uint64_t creation_time;               /**< Timestamp of creation */
} imagination_scenario_t;

/**
 * @brief Reasoning analysis result
 */
typedef struct {
    uint64_t scenario_id;                 /**< Analyzed scenario ID */
    float logical_consistency;            /**< Logic consistency score [0,1] */
    float utility_estimate;               /**< Expected utility [0,1] */
    float confidence;                     /**< Analysis confidence [0,1] */
    bool feasible;                        /**< Whether scenario is feasible */
} imagination_analysis_result_t;

/**
 * @brief Counterfactual scenario for analysis
 *
 * WHAT: Describes a "what-if" scenario for reasoning to analyze
 * WHY: Structured format for counterfactual requests
 * HOW: Contains scenario description and parameters
 */
typedef struct {
    /** Unique scenario identifier */
    uint64_t scenario_id;

    /** Scenario description */
    char description[IMAG_REASON_MAX_SCENARIO_LEN];

    /** Scenario complexity (0-1) */
    float complexity;

    /** Number of variables in scenario */
    uint32_t variable_count;

    /** Timestamp of creation (microseconds) */
    uint64_t timestamp_us;
} counterfactual_scenario_t;

/**
 * @brief Result of counterfactual analysis
 *
 * WHAT: Reasoning's analysis of a counterfactual scenario
 * WHY: Structured response to counterfactual requests
 * HOW: Contains analysis results and confidence
 */
typedef struct {
    /** Original scenario ID */
    uint64_t scenario_id;

    /** Analysis result code (0 = success) */
    int32_t result_code;

    /** Confidence in analysis (0-1) */
    float confidence;

    /** Plausibility score (0-1) */
    float plausibility;

    /** Number of implications derived */
    uint32_t implication_count;

    /** Analysis description */
    char analysis[IMAG_REASON_MAX_SCENARIO_LEN];

    /** Processing time (microseconds) */
    uint64_t processing_time_us;
} counterfactual_result_t;

/**
 * @brief Simulation result from imagination
 *
 * WHAT: Result of mental simulation for reasoning to process
 * WHY: Share simulation outcomes with reasoning module
 * HOW: Contains simulation summary and metrics
 */
typedef struct {
    /** Unique simulation identifier */
    uint64_t simulation_id;

    /** Simulation type */
    uint32_t simulation_type;

    /** Success/failure outcome */
    bool success;

    /** Outcome confidence (0-1) */
    float confidence;

    /** Predicted utility (arbitrary units) */
    float predicted_utility;

    /** Simulation steps taken */
    uint32_t steps;

    /** Result description */
    char description[IMAG_REASON_MAX_SCENARIO_LEN];

    /** Timestamp (microseconds) */
    uint64_t timestamp_us;
} simulation_result_t;

/**
 * @brief Creative inference request
 *
 * WHAT: Request for creative logical inference from premises
 * WHY: Enable novel reasoning from imagined premises
 * HOW: Contains premises and creativity parameters
 */
typedef struct {
    /** Unique request identifier */
    uint64_t request_id;

    /** Number of premises */
    uint32_t premise_count;

    /** Premise descriptions (simplified) */
    char premises[IMAG_REASON_MAX_PREMISES][256];

    /** Desired novelty level (0-1) */
    float novelty_target;

    /** Constraint strictness (0-1, higher = stricter logic) */
    float constraint_strictness;

    /** Timestamp (microseconds) */
    uint64_t timestamp_us;
} creative_inference_request_t;

/**
 * @brief Creative inference result
 *
 * WHAT: Result of creative inference process
 * WHY: Share novel inferences with imagination
 * HOW: Contains inference and quality metrics
 */
typedef struct {
    /** Original request ID */
    uint64_t request_id;

    /** Result code (0 = success) */
    int32_t result_code;

    /** Inference description */
    char inference[IMAG_REASON_MAX_SCENARIO_LEN];

    /** Achieved novelty (0-1) */
    float novelty;

    /** Logical validity (0-1) */
    float validity;

    /** Usefulness estimate (0-1) */
    float usefulness;

    /** Processing time (microseconds) */
    uint64_t processing_time_us;
} creative_inference_result_t;

/**
 * @brief Imagination insight
 *
 * WHAT: Insight generated from imagination process
 * WHY: Share imaginative insights with reasoning
 * HOW: Contains insight content and confidence
 */
typedef struct {
    /** Unique insight identifier */
    uint64_t insight_id;

    /** Insight type */
    uint32_t insight_type;

    /** Insight description */
    char description[IMAG_REASON_MAX_INSIGHT_LEN];

    /** Confidence in insight (0-1) */
    float confidence;

    /** Surprise value (0-1) */
    float surprise;

    /** Relevance to current context (0-1) */
    float relevance;

    /** Timestamp (microseconds) */
    uint64_t timestamp_us;
} imagination_insight_t;

/**
 * @brief Statistics for Imagination-Reasoning bridge
 */
typedef struct {
    uint64_t scenarios_generated;         /**< Total scenarios generated */
    uint64_t scenarios_analyzed;          /**< Scenarios fully analyzed */
    uint64_t scenarios_accepted;          /**< Scenarios deemed feasible */
    uint64_t events_received;             /**< Events received from hub */
    uint64_t events_published;            /**< Events published to hub */
    float avg_plausibility;               /**< Average scenario plausibility */
    float avg_utility;                    /**< Average utility estimate */
    float avg_analysis_time_ms;           /**< Average analysis time */
    uint32_t total_events;                /**< Total events processed */
    uint32_t counterfactual_queries;      /**< Counterfactual queries processed */
    uint32_t simulation_results;          /**< Simulation results processed */
    uint32_t creative_inferences;         /**< Creative inferences generated */
    uint32_t insights_shared;             /**< Insights shared */
    uint32_t queries_handled;             /**< Queries handled */
    uint32_t query_errors;                /**< Query errors */
    float avg_counterfactual_confidence;  /**< Avg counterfactual confidence */
    float avg_creative_novelty;           /**< Avg creative inference novelty */
} imagination_reasoning_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for counterfactual analysis results
 *
 * @param result Analysis result
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
typedef int (*counterfactual_result_callback_t)(
    const counterfactual_result_t* result,
    void* user_data
);

/**
 * @brief Callback for simulation processing
 *
 * @param result Simulation result
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
typedef int (*simulation_result_callback_t)(
    const simulation_result_t* result,
    void* user_data
);

/**
 * @brief Callback for creative inference results
 *
 * @param result Inference result
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
typedef int (*creative_inference_callback_t)(
    const creative_inference_result_t* result,
    void* user_data
);

/**
 * @brief Callback for imagination insights
 *
 * @param insight Imagination insight
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
typedef int (*insight_callback_t)(
    const imagination_insight_t* insight,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_default_config(imagination_reasoning_config_t* config);

/**
 * @brief Create Imagination-Reasoning bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
imagination_reasoning_bridge_t* imagination_reasoning_bridge_create(
    const imagination_reasoning_config_t* config
);

/**
 * @brief Destroy Imagination-Reasoning bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void imagination_reasoning_bridge_destroy(imagination_reasoning_bridge_t* bridge);

/* ============================================================================
 * Hub Registration API
 * ============================================================================ */

/**
 * @brief Register bridge with cognitive hub
 *
 * WHAT: Connect bridge to the cognitive integration hub
 * WHY: Enable event subscription and publication through the hub
 * HOW: Register module, subscribe to configured events
 *
 * @param bridge Bridge instance
 * @param hub Cognitive integration hub
 * @return 0 on success, -1 on error
 *
 * ACTIONS:
 * - Registers bridge as COG_CATEGORY_REASONING module
 * - Subscribes to COG_EVENT_OUTPUT_READY, COG_EVENT_STATE_CHANGE,
 *   COG_EVENT_LEARNING_COMPLETE (based on config)
 * - Registers query handler if enabled
 */
int imagination_reasoning_bridge_register_with_hub(
    imagination_reasoning_bridge_t* bridge,
    cognitive_integration_hub_t hub
);

/**
 * @brief Unregister bridge from cognitive hub
 *
 * WHAT: Disconnect bridge from the cognitive integration hub
 * WHY: Clean shutdown or reconfiguration
 * HOW: Unsubscribe from all events, unregister module
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_unregister_from_hub(
    imagination_reasoning_bridge_t* bridge
);

/**
 * @brief Connect bridge to cognitive hub (alias for register_with_hub)
 *
 * @param bridge Imagination-Reasoning bridge
 * @param hub Cognitive integration hub
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_connect(
    imagination_reasoning_bridge_t* bridge,
    cognitive_integration_hub_t hub
);

/**
 * @brief Disconnect bridge from cognitive hub (alias for unregister_from_hub)
 *
 * @param bridge Imagination-Reasoning bridge
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_disconnect(imagination_reasoning_bridge_t* bridge);

/**
 * @brief Check if bridge is connected/registered
 *
 * @param bridge Imagination-Reasoning bridge
 * @return true if connected, false otherwise
 */
bool imagination_reasoning_bridge_is_connected(const imagination_reasoning_bridge_t* bridge);

/* ============================================================================
 * Module Connection API
 * ============================================================================ */

/**
 * @brief Set imagination engine reference
 *
 * WHAT: Connect bridge to imagination engine
 * WHY: Enable bidirectional communication with imagination
 * HOW: Store engine reference for event routing
 *
 * @param bridge Bridge instance
 * @param engine Imagination engine (NULL to clear)
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_set_imagination(
    imagination_reasoning_bridge_t* bridge,
    imagination_engine_t* engine
);

/**
 * @brief Set reasoning engine reference
 *
 * WHAT: Connect bridge to reasoning engine
 * WHY: Enable bidirectional communication with reasoning
 * HOW: Store engine reference for event routing
 *
 * @param bridge Bridge instance
 * @param engine Reasoning engine (NULL to clear)
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_set_reasoning(
    imagination_reasoning_bridge_t* bridge,
    reasoning_engine_t* engine
);

/* ============================================================================
 * Counterfactual Analysis API
 * ============================================================================ */

/**
 * @brief Request counterfactual analysis
 *
 * WHAT: Ask reasoning to analyze an imagined counterfactual scenario
 * WHY: Enable "what-if" reasoning about imagined alternatives
 * HOW: Publish counterfactual request event, route to reasoning
 *
 * @param bridge Bridge instance
 * @param scenario Counterfactual scenario to analyze
 * @return 0 on success, -1 on error
 *
 * EVENTS: Publishes COG_EVENT_QUERY with counterfactual payload
 */
int imagination_reasoning_request_counterfactual_analysis(
    imagination_reasoning_bridge_t* bridge,
    const counterfactual_scenario_t* scenario
);

/**
 * @brief Set callback for counterfactual results
 *
 * @param bridge Bridge instance
 * @param callback Result callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_set_counterfactual_callback(
    imagination_reasoning_bridge_t* bridge,
    counterfactual_result_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Simulation Result API
 * ============================================================================ */

/**
 * @brief Publish simulation result
 *
 * WHAT: Share mental simulation outcome with reasoning module
 * WHY: Enable reasoning to use simulation results for inference
 * HOW: Publish simulation result event through hub
 *
 * @param bridge Bridge instance
 * @param result Simulation result to share
 * @return 0 on success, -1 on error
 *
 * EVENTS: Publishes COG_EVENT_OUTPUT_READY with simulation payload
 */
int imagination_reasoning_publish_simulation_result(
    imagination_reasoning_bridge_t* bridge,
    const simulation_result_t* result
);

/**
 * @brief Set callback for simulation processing
 *
 * @param bridge Bridge instance
 * @param callback Result callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_set_simulation_callback(
    imagination_reasoning_bridge_t* bridge,
    simulation_result_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Creative Inference API
 * ============================================================================ */

/**
 * @brief Request creative inference
 *
 * WHAT: Ask reasoning for novel logical leaps from imagined premises
 * WHY: Enable creative combinations of imagination and logic
 * HOW: Publish creative inference request, route to reasoning
 *
 * @param bridge Bridge instance
 * @param request Creative inference request
 * @return 0 on success, -1 on error
 *
 * EVENTS: Publishes COG_EVENT_LEARNING_COMPLETE with creative payload
 */
int imagination_reasoning_request_creative_inference(
    imagination_reasoning_bridge_t* bridge,
    const creative_inference_request_t* request
);

/**
 * @brief Set callback for creative inference results
 *
 * @param bridge Bridge instance
 * @param callback Result callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_set_creative_callback(
    imagination_reasoning_bridge_t* bridge,
    creative_inference_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Insight Publication API
 * ============================================================================ */

/**
 * @brief Publish imagination insight
 *
 * WHAT: Share imaginative insight with reasoning module
 * WHY: Enable reasoning to incorporate imaginative discoveries
 * HOW: Publish insight event through hub
 *
 * @param bridge Bridge instance
 * @param insight Imagination insight to share
 * @return 0 on success, -1 on error
 *
 * EVENTS: Publishes COG_EVENT_STATE_CHANGE with insight payload
 */
int imagination_reasoning_publish_insight(
    imagination_reasoning_bridge_t* bridge,
    const imagination_insight_t* insight
);

/**
 * @brief Set callback for insight feedback
 *
 * @param bridge Bridge instance
 * @param callback Insight callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_set_insight_callback(
    imagination_reasoning_bridge_t* bridge,
    insight_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Scenario Generation API
 * ============================================================================ */

/**
 * @brief Generate imagined scenario
 *
 * @param bridge Imagination-Reasoning bridge
 * @param type Scenario type
 * @param complexity Complexity level [0,1]
 * @param scenario_out Output scenario
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_generate_scenario(
    imagination_reasoning_bridge_t* bridge,
    imagination_scenario_type_t type,
    float complexity,
    imagination_scenario_t* scenario_out
);

/**
 * @brief Analyze scenario with reasoning
 *
 * @param bridge Imagination-Reasoning bridge
 * @param scenario Scenario to analyze
 * @param result_out Output analysis result
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_analyze_scenario(
    imagination_reasoning_bridge_t* bridge,
    const imagination_scenario_t* scenario,
    imagination_analysis_result_t* result_out
);

/**
 * @brief Publish scenario result to hub
 *
 * @param bridge Imagination-Reasoning bridge
 * @param result Analysis result to publish
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_publish_result(
    imagination_reasoning_bridge_t* bridge,
    const imagination_analysis_result_t* result
);

/* ============================================================================
 * Event Handling API
 * ============================================================================ */

/**
 * @brief Internal event callback for hub events
 *
 * WHAT: Handle events from cognitive hub
 * WHY: Route events between imagination and reasoning
 * HOW: Dispatch to appropriate handlers based on event type
 *
 * @param event Event data from hub
 * @param user_data Bridge instance
 * @return 0 on success, -1 on error
 *
 * NOTE: This is registered with the hub during bridge registration
 */
int imagination_reasoning_on_event(
    const void* event,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 *
 * @param bridge Imagination-Reasoning bridge
 * @return Current state
 */
imagination_reasoning_state_t imagination_reasoning_bridge_get_state(
    const imagination_reasoning_bridge_t* bridge
);

/**
 * @brief Get module ID
 *
 * @param bridge Imagination-Reasoning bridge
 * @return Module ID, or 0 on error
 */
uint32_t imagination_reasoning_bridge_get_module_id(
    const imagination_reasoning_bridge_t* bridge
);

/**
 * @brief Get active scenario count
 *
 * @param bridge Imagination-Reasoning bridge
 * @return Number of active scenarios
 */
uint32_t imagination_reasoning_bridge_get_active_scenarios(
    const imagination_reasoning_bridge_t* bridge
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Imagination-Reasoning bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_get_stats(
    const imagination_reasoning_bridge_t* bridge,
    imagination_reasoning_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Imagination-Reasoning bridge
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_reset_stats(imagination_reasoning_bridge_t* bridge);

/**
 * @brief Force update bridge state
 *
 * @param bridge Imagination-Reasoning bridge
 * @return 0 on success, -1 on error
 */
int imagination_reasoning_bridge_force_update(imagination_reasoning_bridge_t* bridge);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get string name for bridge event type
 *
 * @param event_type Event type
 * @return Human-readable name or "UNKNOWN"
 */
const char* imag_reason_event_type_to_string(imag_reason_event_type_t event_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_REASONING_BRIDGE_H */
