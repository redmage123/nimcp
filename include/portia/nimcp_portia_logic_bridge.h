//=============================================================================
// nimcp_portia_logic_bridge.h - Portia-Logic Bridge Integration
//=============================================================================
/**
 * @file nimcp_portia_logic_bridge.h
 * @brief Portia-Logic Bridge: Neural Logic Gates for Resource Optimization
 *
 * WHAT: Bridges neural logic gate system with Portia resource optimization
 * WHY:  Enable symbolic logic for tier switching, degradation, and allocation decisions
 * HOW:  Combines logic gates (AND/OR/NOT/XOR/IMPLIES) with Portia resource states
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                   Portia-Logic Bridge                           │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Portia States     ──┐                                         │
 * │  (tier, power, etc) │                                         │
 * │                     ├──►  Logic Gate Network                  │
 * │  Decision Rules  ───┘     (AND/OR/NOT/XOR/IMPLIES)           │
 * │                                 │                              │
 * │                                 ▼                              │
 * │                          Decision Results                      │
 * │                     (upgrade, degrade, allocate)              │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * USE CASES:
 * 1. Tier Switching: AND gates for upgrade conditions
 * 2. Degradation: IMPLIES gates for feature disable rules
 * 3. Resource Allocation: AND/OR gates for budget decisions
 * 4. Emergency Detection: OR gates for critical conditions
 *
 * BIOLOGICAL INSPIRATION:
 * - Portia fimbriata: Resource-constrained decision making (600K neurons)
 * - Neural logic: Symbolic reasoning with biological substrates
 * - Graceful degradation: Progressive feature reduction under constraints
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_LOGIC_BRIDGE_H
#define NIMCP_PORTIA_LOGIC_BRIDGE_H

#include "utils/validation/nimcp_common.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "portia/nimcp_portia.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct portia_logic_bridge portia_logic_bridge_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *============================================================================*/

/**
 * @brief Portia-Logic Bridge configuration
 */
typedef struct {
    uint32_t max_gates;              /**< Maximum logic gates (default: 100) */
    uint32_t max_custom_rules;       /**< Maximum custom rules (default: 50) */
    bool enable_bio_async;           /**< Enable bio-async messaging (default: true) */
    float decision_threshold;        /**< Min confidence for decisions (default: 0.7) */
    uint32_t evaluation_timeout_ms;  /**< Gate evaluation timeout (default: 100) */
    bool enable_brain_integration;   /**< Enable brain neuromodulation (default: false) */
    bool enable_immune_integration;  /**< Enable immune system integration (default: false) */
    bool enable_umm_integration;     /**< Enable UMM integration (default: false) */
    bool disable_auto_update;        /**< Disable auto-update of conditions (for testing) */
} portia_logic_config_t;

/**
 * @brief Resource condition for logic evaluation
 */
typedef struct {
    bool memory_ok;                  /**< Memory within limits */
    bool thermal_ok;                 /**< Temperature safe */
    bool battery_ok;                 /**< Battery sufficient */
    bool cpu_ok;                     /**< CPU not overloaded */
    bool accelerator_available;      /**< Hardware accelerator present */
    bool emergency_mode;             /**< Emergency state active */
    float resource_score;            /**< Overall resource score [0-1] */
} portia_resource_condition_t;

/**
 * @brief Decision gate result
 */
typedef struct {
    uint32_t gate_id;                /**< Gate that produced result */
    bool result;                     /**< Logical result (true/false) */
    float confidence;                /**< Result confidence [0-1] */
    uint64_t evaluation_time_us;     /**< Evaluation time (microseconds) */
    char explanation[256];           /**< Human-readable explanation */
} portia_logic_decision_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;      /**< Total gate evaluations */
    uint64_t successful_evaluations; /**< Successful evaluations */
    uint64_t failed_evaluations;     /**< Failed evaluations */
    uint64_t tier_upgrade_decisions; /**< Tier upgrade evaluations */
    uint64_t tier_downgrade_decisions; /**< Tier downgrade evaluations */
    uint64_t degradation_decisions;  /**< Degradation evaluations */
    uint64_t allocation_decisions;   /**< Allocation evaluations */
    float avg_evaluation_time_us;    /**< Average evaluation time */
    uint32_t active_gates;           /**< Currently active gates */
} portia_logic_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Provides default configuration for portia-logic bridge
 * WHY:  Ensures safe defaults for all parameters
 * HOW:  Returns struct with sensible defaults
 *
 * @param config Configuration to fill with defaults
 */
void portia_logic_bridge_get_default_config(portia_logic_config_t* config);

/**
 * @brief Create portia-logic bridge
 *
 * WHAT: Creates and initializes portia-logic bridge
 * WHY:  Enables symbolic logic for resource optimization
 * HOW:  Allocates memory, initializes neural logic network, creates decision gates
 *
 * @param config Configuration (NULL for defaults)
 * @param portia Portia context to integrate
 * @return Bridge handle or NULL on failure
 */
portia_logic_bridge_t* portia_logic_bridge_create(
    const portia_logic_config_t* config,
    portia_context_t* portia
);

/**
 * @brief Destroy portia-logic bridge
 *
 * WHAT: Cleans up and destroys portia-logic bridge
 * WHY:  Prevents memory leaks
 * HOW:  Unregisters from bio-async, destroys logic network, frees memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void portia_logic_bridge_destroy(portia_logic_bridge_t* bridge);

/**
 * @brief Start bridge operation
 *
 * WHAT: Activates bridge for decision making
 * WHY:  Enable real-time logic evaluation
 * HOW:  Registers with bio-async, initializes gates
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_bridge_start(portia_logic_bridge_t* bridge);

/**
 * @brief Stop bridge operation
 *
 * WHAT: Deactivates bridge
 * WHY:  Clean shutdown
 * HOW:  Unregisters from bio-async
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_bridge_stop(portia_logic_bridge_t* bridge);

/*=============================================================================
 * INTEGRATION FUNCTIONS
 *============================================================================*/

/**
 * @brief Connect to brain for neuromodulation
 *
 * WHAT: Integrates with brain for DA/ACh modulation of logic gates
 * WHY:  Enable neurochemical influence on decision flexibility
 * HOW:  Stores brain reference, enables neuromodulation in logic network
 *
 * @param bridge Bridge handle
 * @param brain Brain instance
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_connect_brain(portia_logic_bridge_t* bridge, brain_t brain);

/**
 * @brief Connect to immune system
 *
 * WHAT: Integrates with brain immune system
 * WHY:  Enable immune state to influence logic decisions
 * HOW:  Stores immune reference for inflammation-aware decisions
 *
 * @param bridge Bridge handle
 * @param immune_system Brain immune system instance
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_connect_immune(portia_logic_bridge_t* bridge, void* immune_system);

/**
 * @brief Connect to UMM (Unified Memory Manager)
 *
 * WHAT: Integrates with UMM for memory state
 * WHY:  Enable memory-aware resource decisions
 * HOW:  Stores UMM reference for memory condition evaluation
 *
 * @param bridge Bridge handle
 * @param umm UMM instance
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_connect_umm(portia_logic_bridge_t* bridge, void* umm);

/*=============================================================================
 * DECISION EVALUATION FUNCTIONS
 *============================================================================*/

/**
 * @brief Evaluate tier upgrade decision
 *
 * WHAT: Determines if tier upgrade is allowed
 * WHY:  Ensure safe tier transitions based on resource state
 * HOW:  Evaluates AND gate: (memory_ok AND thermal_ok AND battery_ok)
 *
 * @param bridge Bridge handle
 * @param current_tier Current platform tier
 * @param target_tier Target tier to upgrade to
 * @return true if upgrade allowed, false otherwise
 *
 * BIOLOGICAL BASIS:
 * - Portia fimbriata: Only attempts complex hunting when energy sufficient
 * - Neural coincidence detection: All conditions must be met
 */
bool portia_logic_can_upgrade_tier(
    portia_logic_bridge_t* bridge,
    uint8_t current_tier,
    uint8_t target_tier
);

/**
 * @brief Evaluate tier downgrade necessity
 *
 * WHAT: Determines if tier downgrade is required
 * WHY:  Prevent resource exhaustion through proactive adaptation
 * HOW:  Evaluates OR gate: (memory_critical OR thermal_critical OR battery_critical)
 *
 * @param bridge Bridge handle
 * @param current_tier Current platform tier
 * @return true if downgrade required, false otherwise
 */
bool portia_logic_must_downgrade_tier(
    portia_logic_bridge_t* bridge,
    uint8_t current_tier
);

/**
 * @brief Evaluate feature degradation decision
 *
 * WHAT: Determines if feature can be disabled
 * WHY:  Enable graceful degradation under constraints
 * HOW:  Evaluates IMPLIES gate: (resource_critical IMPLIES disable_non_essential)
 *       AND gate: (feature_active AND NOT is_core)
 *
 * @param bridge Bridge handle
 * @param feature_id Feature identifier to evaluate
 * @return true if feature can be disabled, false otherwise
 */
bool portia_logic_can_disable_feature(
    portia_logic_bridge_t* bridge,
    uint32_t feature_id
);

/**
 * @brief Evaluate resource allocation decision
 *
 * WHAT: Determines if resource allocation is allowed
 * WHY:  Prevent over-allocation and resource conflicts
 * HOW:  Evaluates AND gate: (budget_available AND request_valid AND priority_ok)
 *
 * @param bridge Bridge handle
 * @param target_id Target module requesting resources
 * @param amount Amount of resource requested
 * @return true if allocation allowed, false otherwise
 */
bool portia_logic_can_allocate_resource(
    portia_logic_bridge_t* bridge,
    uint32_t target_id,
    float amount
);

/*=============================================================================
 * GATE MANAGEMENT
 *============================================================================*/

/**
 * @brief Add custom logic gate
 *
 * WHAT: Adds user-defined logic gate to bridge
 * WHY:  Enable custom decision rules
 * HOW:  Parses expression, creates gate in neural logic network
 *
 * @param bridge Bridge handle
 * @param expression Logic expression (e.g., "A AND B", "NOT C")
 * @param gate_id_out Output gate ID
 * @return NIMCP_SUCCESS or error code
 *
 * SUPPORTED EXPRESSIONS:
 * - "A AND B" - Conjunction
 * - "A OR B" - Disjunction
 * - "NOT A" - Negation
 * - "A XOR B" - Exclusive OR
 * - "A IMPLIES B" - Implication
 */
int portia_logic_add_custom_gate(
    portia_logic_bridge_t* bridge,
    const char* expression,
    uint32_t* gate_id_out
);

/**
 * @brief Evaluate custom gate
 *
 * WHAT: Evaluates specific gate by ID
 * WHY:  Allow manual evaluation of custom rules
 * HOW:  Queries neural logic network, returns result
 *
 * @param bridge Bridge handle
 * @param gate_id Gate ID to evaluate
 * @return true if gate evaluates to true, false otherwise
 */
bool portia_logic_evaluate_gate(portia_logic_bridge_t* bridge, uint32_t gate_id);

/**
 * @brief Get gate decision with details
 *
 * WHAT: Evaluates gate and returns full decision info
 * WHY:  Enable debugging and explanation of decisions
 * HOW:  Evaluates gate, populates decision structure
 *
 * @param bridge Bridge handle
 * @param gate_id Gate ID to evaluate
 * @param decision Output decision structure
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_get_gate_decision(
    portia_logic_bridge_t* bridge,
    uint32_t gate_id,
    portia_logic_decision_t* decision
);

/*=============================================================================
 * RESOURCE CONDITION FUNCTIONS
 *============================================================================*/

/**
 * @brief Update resource conditions from Portia state
 *
 * WHAT: Synchronizes resource conditions with current Portia state
 * WHY:  Ensure logic gates operate on fresh data
 * HOW:  Queries Portia, updates internal resource condition cache
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_update_conditions(portia_logic_bridge_t* bridge);

/**
 * @brief Get current resource conditions
 *
 * WHAT: Retrieves current resource condition state
 * WHY:  Allow external inspection of decision inputs
 * HOW:  Returns cached resource conditions
 *
 * @param bridge Bridge handle
 * @param conditions Output conditions structure
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_get_conditions(
    const portia_logic_bridge_t* bridge,
    portia_resource_condition_t* conditions
);

/**
 * @brief Manually set resource condition
 *
 * WHAT: Overrides specific resource condition
 * WHY:  Enable testing and manual control
 * HOW:  Updates internal condition state
 *
 * @param bridge Bridge handle
 * @param condition_name Condition to set (e.g., "memory_ok")
 * @param value Condition value (true/false)
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_set_condition(
    portia_logic_bridge_t* bridge,
    const char* condition_name,
    bool value
);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

/**
 * @brief Connect to bio-async messaging system
 *
 * WHAT: Registers bridge with bio-async router
 * WHY:  Enable inter-module messaging
 * HOW:  Calls bio_router_register_module with bridge context
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_connect_bio_async(portia_logic_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging system
 *
 * WHAT: Unregisters bridge from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Calls bio_router_unregister_module
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_disconnect_bio_async(portia_logic_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Queries bio-async connection status
 * WHY:  Verify integration state
 * HOW:  Checks internal bio_async_enabled flag
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool portia_logic_is_bio_async_connected(const portia_logic_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Processes bio-async inbox
 * WHY:  Handle incoming messages from other modules
 * HOW:  Calls bio_router_process_inbox
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, -1 on error
 */
int portia_logic_process_inbox(portia_logic_bridge_t* bridge);

/**
 * @brief Broadcast decision result
 *
 * WHAT: Sends decision result to interested modules
 * WHY:  Enable distributed decision awareness
 * HOW:  Constructs bio-async message, broadcasts to subscribers
 *
 * @param bridge Bridge handle
 * @param decision Decision to broadcast
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_broadcast_decision(
    portia_logic_bridge_t* bridge,
    const portia_logic_decision_t* decision
);

/*=============================================================================
 * STATISTICS AND DEBUGGING
 *============================================================================*/

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves bridge performance statistics
 * WHY:  Enable monitoring and debugging
 * HOW:  Returns snapshot of internal counters
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_get_stats(
    const portia_logic_bridge_t* bridge,
    portia_logic_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Resets all statistics counters to zero
 * WHY:  Enable periodic monitoring
 * HOW:  Zeros all counters in stats structure
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
int portia_logic_reset_stats(portia_logic_bridge_t* bridge);

/**
 * @brief Get gate count
 *
 * WHAT: Returns number of active gates
 * WHY:  Monitor gate usage
 * HOW:  Queries neural logic network
 *
 * @param bridge Bridge handle
 * @return Number of active gates
 */
uint32_t portia_logic_get_gate_count(const portia_logic_bridge_t* bridge);

/**
 * @brief Dump gate state for debugging
 *
 * WHAT: Prints all gate states to log
 * WHY:  Debug decision logic
 * HOW:  Iterates gates, logs state
 *
 * @param bridge Bridge handle
 */
void portia_logic_dump_gates(const portia_logic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_LOGIC_BRIDGE_H */
