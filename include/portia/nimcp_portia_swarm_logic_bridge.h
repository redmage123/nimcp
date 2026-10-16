//=============================================================================
// nimcp_portia_swarm_logic_bridge.h - Unified Portia-Swarm-Logic Bridge
//=============================================================================
/**
 * @file nimcp_portia_swarm_logic_bridge.h
 * @brief Unified integration of Portia adaptive intelligence, Swarm collective behavior, and Neural Logic gates
 *
 * WHAT: Mediator bridge coordinating decisions across Portia (individual adaptation),
 *       Swarm (collective intelligence), and Logic (symbolic reasoning) systems
 * WHY:  Enable coherent decision-making where local resource constraints, collective
 *       consensus, and logical validation all influence system behavior
 * HOW:  Unified decision gates combining all three systems with configurable weights,
 *       bio-async messaging, and integration with brain/immune/UMM systems
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │           Unified Portia-Swarm-Logic Bridge                     │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Portia Logic ───┐                                             │
 * │  (Local Rules)   │                                             │
 * │                  ├──►  Unified Decision Gates                  │
 * │  Swarm Logic ────┤     (AND/OR/IMPLIES)                       │
 * │  (Collective)    │           │                                 │
 * │                  │           ▼                                 │
 * │  Portia-Swarm ───┘     Coordinated Actions                    │
 * │  (Resource Sync)       (tier switch, degradation, resources)   │
 * │                                                                 │
 * │  Brain Integration: Neuromodulation affects thresholds         │
 * │  Immune Integration: Inflammation modulates decisions          │
 * │  UMM Integration: Memory pressure influences gates             │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * DECISION PATTERNS:
 * 1. Collective Resource Allocation:
 *    AND gate: (local_resources_ok AND swarm_consensus) → proceed
 *
 * 2. Emergency Mode Activation:
 *    OR gate: (local_critical OR swarm_alert) → emergency
 *
 * 3. Swarm-Influenced Tier Switching:
 *    IMPLIES gate: (swarm_recommends_upgrade IMPLIES local_can_upgrade)
 *
 * 4. Coordinated Degradation:
 *    OR gate: (local_degradation OR swarm_degradation_signal) → coordinate
 *
 * BIOLOGICAL BASIS:
 * - Portia spider: Individual resource optimization (600K neurons)
 * - Swarm intelligence: Collective decision-making (ant colonies, bee swarms)
 * - Neural logic: Coincidence detection, conditional firing (prefrontal cortex)
 * - Integration: How individual constraints, collective wisdom, and logical
 *   reasoning combine in distributed neural systems
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_SWARM_LOGIC_BRIDGE_H
#define NIMCP_PORTIA_SWARM_LOGIC_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_logic_bridge portia_logic_bridge_t;
typedef struct swarm_logic_bridge swarm_logic_bridge_t;
typedef struct portia_swarm_bridge_t portia_swarm_bridge_t;
typedef struct brain_struct* brain_t;
typedef struct perception_training_bridge perception_training_bridge_t;
typedef struct cortical_training_bridge cortical_training_bridge_t;

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module ID for unified bridge */
#define BIO_MODULE_PORTIA_SWARM_LOGIC 0x0C30

/** Maximum decision reason string length */
#define PSL_MAX_REASON_LENGTH 256

/** Default consensus timeout in milliseconds */
#define PSL_DEFAULT_CONSENSUS_TIMEOUT_MS 1000

/** Default confidence threshold for decisions */
#define PSL_DEFAULT_CONFIDENCE_THRESHOLD 0.7f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Operation mode for unified bridge
 */
typedef enum {
    PSL_MODE_DISABLED = 0,           /**< Integration disabled */
    PSL_MODE_PORTIA_ONLY = 1,        /**< Only Portia logic gates */
    PSL_MODE_SWARM_ONLY = 2,         /**< Only Swarm logic gates */
    PSL_MODE_COORDINATED = 3,        /**< Both with coordination */
    PSL_MODE_CONSENSUS_REQUIRED = 4  /**< Require swarm consensus for Portia decisions */
} portia_swarm_logic_mode_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for unified bridge
 */
typedef struct {
    portia_swarm_logic_mode_t mode;  /**< Operation mode */
    float local_weight;              /**< Weight for local (Portia) decisions [0-1] */
    float collective_weight;         /**< Weight for collective (Swarm) decisions [0-1] */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_brain_integration;   /**< Enable brain neuromodulation */
    bool enable_immune_feedback;     /**< Enable immune system feedback */
    bool enable_umm_tracking;        /**< Enable UMM memory tracking */
    uint32_t consensus_timeout_ms;   /**< Timeout for consensus operations */
    float confidence_threshold;      /**< Minimum confidence for decisions [0-1] */
} portia_swarm_logic_config_t;

/**
 * @brief Statistics for unified bridge
 */
typedef struct {
    uint64_t total_decisions;        /**< Total decisions made */
    uint64_t local_decisions;        /**< Decisions from local (Portia) only */
    uint64_t collective_decisions;   /**< Decisions from collective (Swarm) only */
    uint64_t consensus_achieved;     /**< Successful consensus operations */
    uint64_t consensus_failed;       /**< Failed consensus operations */
    uint64_t emergency_activations;  /**< Emergency mode activations */
    float avg_decision_time_us;      /**< Average decision time in microseconds */
    float avg_consensus_confidence;  /**< Average consensus confidence [0-1] */

    /* Perception/Cortical integration stats */
    bool perception_training_connected;  /**< Perception training bridge connected */
    bool cortical_training_connected;    /**< Cortical training bridge connected */
    uint64_t perception_influenced_decisions; /**< Decisions influenced by perception */
    uint64_t cortical_influenced_decisions;   /**< Decisions influenced by cortical */
} portia_swarm_logic_stats_t;

/**
 * @brief Result of unified decision
 */
typedef struct {
    bool approved;                   /**< Decision approved */
    float confidence;                /**< Overall confidence [0-1] */
    bool local_approved;             /**< Local (Portia) approved */
    bool swarm_approved;             /**< Swarm consensus approved */
    uint32_t swarm_consensus_count;  /**< Number of agents in consensus */
    char reason[PSL_MAX_REASON_LENGTH]; /**< Human-readable reason */
    uint64_t decision_time_us;       /**< Time taken for decision (microseconds) */
} unified_decision_result_t;

/**
 * @brief Main bridge context (opaque)
 */
typedef struct portia_swarm_logic_bridge portia_swarm_logic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Initialize configuration with defaults
 *
 * WHAT: Provides default configuration for unified bridge
 * WHY:  Ensures safe defaults for all parameters
 * HOW:  Sets mode to COORDINATED, equal weights, standard timeouts
 *
 * @param config Configuration structure to initialize
 */
void portia_swarm_logic_default_config(portia_swarm_logic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create unified Portia-Swarm-Logic bridge
 *
 * WHAT: Creates and initializes unified decision bridge
 * WHY:  Enable coordinated decision-making across three systems
 * HOW:  Allocates memory, connects bridges, creates unified gates
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param portia_logic Portia logic bridge (may be NULL)
 * @param swarm_logic Swarm logic bridge (may be NULL)
 * @param portia_swarm Portia-Swarm bridge (may be NULL)
 * @return Bridge handle or NULL on failure
 */
portia_swarm_logic_bridge_t* portia_swarm_logic_create(
    const portia_swarm_logic_config_t* config,
    portia_logic_bridge_t* portia_logic,
    swarm_logic_bridge_t* swarm_logic,
    portia_swarm_bridge_t* portia_swarm
);

/**
 * @brief Destroy unified bridge and free resources
 *
 * WHAT: Cleans up and destroys unified bridge
 * WHY:  Prevents memory leaks
 * HOW:  Unregisters from bio-async, frees memory (NULL-safe)
 *
 * @param bridge Bridge to destroy
 */
void portia_swarm_logic_destroy(portia_swarm_logic_bridge_t* bridge);

/**
 * @brief Start bridge operation
 *
 * WHAT: Activates bridge and begins processing
 * WHY:  Enable decision-making and message handling
 * HOW:  Registers with bio-async, initializes gates
 *
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_start(portia_swarm_logic_bridge_t* bridge);

/**
 * @brief Stop bridge operation
 *
 * WHAT: Deactivates bridge and stops processing
 * WHY:  Clean shutdown of decision systems
 * HOW:  Unregisters from bio-async, flushes pending operations
 *
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_stop(portia_swarm_logic_bridge_t* bridge);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect brain for neuromodulation
 *
 * WHAT: Integrates brain neuromodulators with decision thresholds
 * WHY:  Enable dopamine/acetylcholine to modulate logic gates
 * HOW:  Stores brain reference, updates gate parameters based on NT levels
 *
 * BIOLOGICAL BASIS:
 * - Dopamine: Modulates flexibility vs rigidity of decisions
 * - Acetylcholine: Modulates precision of logical reasoning
 *
 * @param bridge Bridge context
 * @param brain Brain instance
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_connect_brain(
    portia_swarm_logic_bridge_t* bridge,
    brain_t brain
);

/**
 * @brief Connect immune system for inflammation feedback
 *
 * WHAT: Integrates brain immune system with decision modulation
 * WHY:  Enable inflammation to affect decision thresholds (fever model)
 * HOW:  Stores immune reference, adjusts weights based on cytokine levels
 *
 * BIOLOGICAL BASIS:
 * - Inflammation reduces cognitive flexibility
 * - Cytokines modulate decision-making circuits
 *
 * @param bridge Bridge context
 * @param immune_system Brain immune system instance
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_connect_immune(
    portia_swarm_logic_bridge_t* bridge,
    void* immune_system
);

/**
 * @brief Connect UMM for memory pressure tracking
 *
 * WHAT: Integrates Unified Memory Manager for resource awareness
 * WHY:  Enable memory pressure to influence resource allocation decisions
 * HOW:  Stores UMM reference, queries memory state during decisions
 *
 * @param bridge Bridge context
 * @param umm UMM instance
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_connect_umm(
    portia_swarm_logic_bridge_t* bridge,
    void* umm
);

/**
 * @brief Connect perception training bridge
 *
 * WHAT: Integrates perception training for decision modulation
 * WHY:  Perception quality affects resource/emergency gate decisions
 * HOW:  Stores bridge reference, queries perception effects during decisions
 *
 * BIOLOGICAL BASIS: Perceptual clarity influences decision confidence.
 * High perception quality → more confident decisions. Low perception
 * quality → reduced consensus threshold (proceed cautiously).
 *
 * @param bridge Bridge context
 * @param perception_training Perception training bridge (NULL to disconnect)
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_connect_perception_training(
    portia_swarm_logic_bridge_t* bridge,
    perception_training_bridge_t* perception_training
);

/**
 * @brief Connect cortical training bridge
 *
 * WHAT: Integrates cortical training for decision modulation
 * WHY:  Cortical stability affects decision thresholds and consensus
 * HOW:  Stores bridge reference, queries cortical effects during decisions
 *
 * BIOLOGICAL BASIS: Stable predictions → reliable decision making.
 * High burst rate → confident symbolic reasoning. Prediction failure
 * → cautious decisions (increase consensus requirement).
 *
 * @param bridge Bridge context
 * @param cortical_training Cortical training bridge (NULL to disconnect)
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_connect_cortical_training(
    portia_swarm_logic_bridge_t* bridge,
    cortical_training_bridge_t* cortical_training
);

/**
 * @brief Get perception-based decision confidence modifier
 *
 * WHAT: Compute confidence modifier from perception state
 * WHY:  Perception quality affects decision confidence
 * HOW:  Uses perception effects to compute confidence multiplier
 *
 * @param bridge Bridge context
 * @return Confidence modifier [0.5-1.5], 1.0 if not connected
 */
float portia_swarm_logic_get_perception_confidence_modifier(
    const portia_swarm_logic_bridge_t* bridge
);

/**
 * @brief Get cortical-based decision threshold modifier
 *
 * WHAT: Compute threshold modifier from cortical state
 * WHY:  Cortical stability affects decision thresholds
 * HOW:  Uses cortical effects to compute threshold multiplier
 *
 * @param bridge Bridge context
 * @return Threshold modifier [0.7-1.3], 1.0 if not connected
 */
float portia_swarm_logic_get_cortical_threshold_modifier(
    const portia_swarm_logic_bridge_t* bridge
);

//=============================================================================
// Unified Decision API
//=============================================================================

/**
 * @brief Decide on platform tier change
 *
 * WHAT: Evaluates tier change using unified local + swarm logic
 * WHY:  Coordinate tier switches across individual and collective needs
 * HOW:  IMPLIES gate: (swarm_recommends IMPLIES local_can) AND local_metrics
 *
 * @param bridge Bridge context
 * @param current_tier Current platform tier
 * @param proposed_tier Proposed new tier
 * @param result Output unified decision result
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_decide_tier_change(
    portia_swarm_logic_bridge_t* bridge,
    uint8_t current_tier,
    uint8_t proposed_tier,
    unified_decision_result_t* result
);

/**
 * @brief Decide on feature degradation
 *
 * WHAT: Evaluates graceful degradation using unified logic
 * WHY:  Coordinate degradation across local resource state and swarm signals
 * HOW:  OR gate: (local_degradation OR swarm_degradation_signal) → degrade
 *
 * @param bridge Bridge context
 * @param feature_id Feature identifier to potentially degrade
 * @param result Output unified decision result
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_decide_degradation(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t feature_id,
    unified_decision_result_t* result
);

/**
 * @brief Decide on resource allocation
 *
 * WHAT: Evaluates resource allocation using unified consensus
 * WHY:  Ensure fair resource distribution across swarm with local constraints
 * HOW:  AND gate: (local_resources_ok AND swarm_consensus) → allocate
 *
 * @param bridge Bridge context
 * @param target_id Target resource consumer ID
 * @param requested_amount Amount of resource requested [0-1]
 * @param result Output unified decision result
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_decide_resource_allocation(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t target_id,
    float requested_amount,
    unified_decision_result_t* result
);

/**
 * @brief Decide on emergency mode activation
 *
 * WHAT: Evaluates emergency mode using multi-condition OR gate
 * WHY:  Enable rapid response to critical conditions from any source
 * HOW:  OR gate: (local_critical OR swarm_alert OR immune_storm) → emergency
 *
 * @param bridge Bridge context
 * @param result Output unified decision result
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_decide_emergency_mode(
    portia_swarm_logic_bridge_t* bridge,
    unified_decision_result_t* result
);

//=============================================================================
// Custom Gate API
//=============================================================================

/**
 * @brief Add custom unified logic gate
 *
 * WHAT: Creates custom decision gate with user-defined logic
 * WHY:  Enable extensible decision patterns beyond built-in gates
 * HOW:  Parses expression, creates neural logic gate, stores mapping
 *
 * @param bridge Bridge context
 * @param expression Logic expression (e.g., "A AND B", "A IMPLIES B")
 * @param gate_id_out Output gate identifier
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_add_unified_gate(
    portia_swarm_logic_bridge_t* bridge,
    const char* expression,
    uint32_t* gate_id_out
);

/**
 * @brief Evaluate custom unified gate
 *
 * WHAT: Evaluates previously created custom gate
 * WHY:  Execute user-defined decision logic
 * HOW:  Looks up gate, evaluates neural logic, returns boolean result
 *
 * @param bridge Bridge context
 * @param gate_id Gate identifier from add_unified_gate
 * @return true if gate evaluates to true, false otherwise
 */
bool portia_swarm_logic_evaluate_unified_gate(
    portia_swarm_logic_bridge_t* bridge,
    uint32_t gate_id
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async messaging system
 *
 * WHAT: Registers bridge with bio-async router
 * WHY:  Enable asynchronous communication with other modules
 * HOW:  Creates module context, registers with BIO_MODULE_PORTIA_SWARM_LOGIC
 *
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_connect_bio_async(portia_swarm_logic_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging system
 *
 * WHAT: Unregisters bridge from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregisters module context, clears bio_async_enabled flag
 *
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_disconnect_bio_async(portia_swarm_logic_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Queries bio-async connection state
 * WHY:  Verify messaging capability before sending
 * HOW:  Returns bio_async_enabled flag
 *
 * @param bridge Bridge context
 * @return true if connected, false otherwise
 */
bool portia_swarm_logic_is_bio_async_connected(const portia_swarm_logic_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Processes messages from bio-async inbox
 * WHY:  Handle requests and notifications from other modules
 * HOW:  Calls bio_router_process_inbox, handles message types
 *
 * @param bridge Bridge context
 * @return Number of messages processed, negative on error
 */
int portia_swarm_logic_process_inbox(portia_swarm_logic_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves operational statistics
 * WHY:  Enable monitoring and debugging
 * HOW:  Copies internal statistics to output structure
 *
 * @param bridge Bridge context
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_get_stats(
    const portia_swarm_logic_bridge_t* bridge,
    portia_swarm_logic_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Resets all statistics to zero
 * WHY:  Enable periodic monitoring windows
 * HOW:  Zeros all counters in internal stats structure
 *
 * @param bridge Bridge context
 * @return 0 on success, negative error code on failure
 */
int portia_swarm_logic_reset_stats(portia_swarm_logic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_SWARM_LOGIC_BRIDGE_H */
