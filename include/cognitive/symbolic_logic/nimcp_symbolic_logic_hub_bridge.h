/**
 * @file nimcp_symbolic_logic_hub_bridge.h
 * @brief Symbolic Logic - Cognitive Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Integration bridge connecting symbolic logic to the cognitive integration hub
 * WHY:  Enable event-driven communication between symbolic logic and other cognitive modules
 * HOW:  Subscribe to relevant events, publish inference results, handle inter-module queries
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREFRONTAL-HIPPOCAMPAL INTEGRATION:
 * -----------------------------------
 * Symbolic reasoning in PFC requires integration with:
 * - Hippocampus: Memory retrieval for fact access
 * - Executive: Decision-making based on logical conclusions
 * - Attention: Focus on relevant logical elements
 * - Emotion: Valence influences rule priority
 *
 * EVENT-DRIVEN INFERENCE:
 * -----------------------
 * - Novel facts trigger forward chaining exploration
 * - Goals from executive trigger backward chaining
 * - Memory consolidation triggers knowledge base updates
 * - Emotional salience modulates rule weights
 *
 * CROSS-MODULE COORDINATION:
 * --------------------------
 * - Reasoning results inform executive decisions
 * - Curiosity drives exploratory inference
 * - Ethics constrains allowable conclusions
 * - Theory of Mind applies logic to social reasoning
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_HUB_BRIDGE_H
#define NIMCP_SYMBOLIC_LOGIC_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOGIC_HUB_MODULE_ID          0x1620  /**< Symbolic logic hub module ID */
#define LOGIC_HUB_MODULE_NAME        "symbolic_logic"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct symbolic_logic_hub_bridge symbolic_logic_hub_bridge_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for symbolic logic hub bridge
 */
typedef struct {
    /* Event subscription enables */
    bool subscribe_memory_access;      /**< Subscribe to memory events */
    bool subscribe_attention_shift;    /**< Subscribe to attention events */
    bool subscribe_emotion_update;     /**< Subscribe to emotion events */
    bool subscribe_decision_made;      /**< Subscribe to executive events */
    bool subscribe_learning_complete;  /**< Subscribe to learning events */

    /* Event publishing enables */
    bool publish_inference_results;    /**< Publish inference completions */
    bool publish_fact_additions;       /**< Publish new fact additions */
    bool publish_contradiction_found;  /**< Publish contradiction detections */

    /* Processing parameters */
    float inference_priority;          /**< Priority for inference events [0,1] */
    uint32_t max_forward_chain_depth;  /**< Max forward chaining depth */
    uint32_t max_backward_chain_depth; /**< Max backward chaining depth */
    float novelty_threshold;           /**< Novelty threshold for exploration */

    /* Integration enables */
    bool enable_memory_tagging;        /**< Tag memories with logical associations */
    bool enable_attention_bias;        /**< Bias attention based on logical salience */
    bool enable_emotional_weights;     /**< Apply emotional weights to rules */
} symbolic_logic_hub_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for symbolic logic hub bridge
 */
typedef struct {
    uint64_t events_received;          /**< Total events received */
    uint64_t events_published;         /**< Total events published */
    uint64_t inferences_triggered;     /**< Inferences triggered by events */
    uint64_t facts_added;              /**< Facts added from events */
    uint64_t queries_answered;         /**< Queries answered */
    uint64_t contradictions_found;     /**< Contradictions detected */
    float avg_inference_latency_ms;    /**< Average inference latency */
} symbolic_logic_hub_stats_t;

/* ============================================================================
 * State
 * ============================================================================ */

/**
 * @brief State of symbolic logic hub bridge
 */
typedef struct {
    bool is_registered;                /**< Registered with hub */
    bool is_active;                    /**< Currently processing events */
    uint32_t pending_inferences;       /**< Pending inference tasks */
    uint64_t last_inference_time;      /**< Last inference timestamp */
    float current_load;                /**< Current processing load [0,1] */
} symbolic_logic_hub_state_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Symbolic logic hub bridge structure
 */
struct symbolic_logic_hub_bridge {
    bridge_base_t base;                /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    symbolic_logic_hub_config_t config;

    /* Connections */
    cognitive_integration_hub_t hub;   /**< Cognitive hub connection */
    symbolic_logic_t* logic;           /**< Symbolic logic system */

    /* State and statistics */
    symbolic_logic_hub_state_t state;
    symbolic_logic_hub_stats_t stats;

    /* Internal tracking */
    uint32_t module_id;                /**< Assigned module ID */
    uint64_t subscription_mask;        /**< Active subscriptions */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int symbolic_logic_hub_bridge_default_config(symbolic_logic_hub_config_t* config);

/**
 * @brief Create symbolic logic hub bridge
 */
symbolic_logic_hub_bridge_t* symbolic_logic_hub_bridge_create(
    const symbolic_logic_hub_config_t* config);

/**
 * @brief Destroy symbolic logic hub bridge
 */
void symbolic_logic_hub_bridge_destroy(symbolic_logic_hub_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to cognitive hub
 */
int symbolic_logic_hub_bridge_connect_hub(
    symbolic_logic_hub_bridge_t* bridge,
    cognitive_integration_hub_t hub);

/**
 * @brief Connect to symbolic logic system
 */
int symbolic_logic_hub_bridge_connect_logic(
    symbolic_logic_hub_bridge_t* bridge,
    symbolic_logic_t* logic);

/**
 * @brief Disconnect from hub
 */
int symbolic_logic_hub_bridge_disconnect(symbolic_logic_hub_bridge_t* bridge);

/* ============================================================================
 * Event Handling API
 * ============================================================================ */

/**
 * @brief Handle incoming event from hub
 */
int symbolic_logic_hub_bridge_handle_event(
    symbolic_logic_hub_bridge_t* bridge,
    const cognitive_event_data_t* event);

/**
 * @brief Publish inference result to hub
 */
int symbolic_logic_hub_bridge_publish_inference(
    symbolic_logic_hub_bridge_t* bridge,
    const char* conclusion,
    float confidence);

/**
 * @brief Publish contradiction detection to hub
 */
int symbolic_logic_hub_bridge_publish_contradiction(
    symbolic_logic_hub_bridge_t* bridge,
    const char* fact1,
    const char* fact2);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Handle query from another module
 */
int symbolic_logic_hub_bridge_handle_query(
    symbolic_logic_hub_bridge_t* bridge,
    cognitive_query_type_t query_type,
    const void* query_data,
    void* result);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge state
 */
int symbolic_logic_hub_bridge_update(
    symbolic_logic_hub_bridge_t* bridge,
    uint64_t delta_ms);

/**
 * @brief Force immediate processing of pending inferences
 */
int symbolic_logic_hub_bridge_force_update(symbolic_logic_hub_bridge_t* bridge);

/* ============================================================================
 * State and Stats API
 * ============================================================================ */

/**
 * @brief Get bridge state
 */
int symbolic_logic_hub_bridge_get_state(
    const symbolic_logic_hub_bridge_t* bridge,
    symbolic_logic_hub_state_t* state);

/**
 * @brief Get bridge statistics
 */
int symbolic_logic_hub_bridge_get_stats(
    const symbolic_logic_hub_bridge_t* bridge,
    symbolic_logic_hub_stats_t* stats);

/**
 * @brief Reset statistics
 */
int symbolic_logic_hub_bridge_reset_stats(symbolic_logic_hub_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 */
int symbolic_logic_hub_bridge_connect_bio_async(symbolic_logic_hub_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int symbolic_logic_hub_bridge_disconnect_bio_async(symbolic_logic_hub_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool symbolic_logic_hub_bridge_is_bio_async_connected(
    const symbolic_logic_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYMBOLIC_LOGIC_HUB_BRIDGE_H */
