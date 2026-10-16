/**
 * @file nimcp_reasoning_substrate_bridge.h
 * @brief Bridge between reasoning system and neural substrate
 *
 * WHAT: Bidirectional integration linking reasoning processes to metabolic/energy state
 * WHY: Reasoning depends on prefrontal-parietal networks with high ATP demands
 * HOW: Monitors ATP, fatigue, metabolic stress; modulates inference depth, accuracy, speed
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (PFC) and parietal networks drive abstract reasoning
 * - Reasoning requires sustained neural firing → high ATP consumption
 * - ATP depletion reduces inference chain depth (shallow thinking)
 * - Metabolic stress impairs working memory → lower logical accuracy
 * - Fatigue slows processing speed and limits abstraction capacity
 * - Executive dysfunction emerges when substrate resources are depleted
 *
 * SUBSTRATE DEPENDENCIES:
 * - ATP: Depletion below 50% reduces inference depth, below 30% causes impairment
 * - Fatigue: High fatigue (>0.7) slows reasoning speed by up to 50%
 * - Metabolic stress: Stress >0.6 impairs abstraction and logical operations
 * - Neural integrity: Damaged networks reduce reasoning reliability
 */

#ifndef NIMCP_REASONING_SUBSTRATE_BRIDGE_H
#define NIMCP_REASONING_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Use symbolic_logic_t as the reasoning system type */
typedef symbolic_logic_t nimcp_reasoning_system_t;

/* ============================================================================
 * Constants
 * ========================================================================== */

/**
 * Bio-async module ID for reasoning substrate bridge
 * Range: 0x1200-0x12FF (Substrate bridges)
 */
#define BIO_MODULE_SUBSTRATE_REASONING 0x1204

/**
 * ATP thresholds for reasoning operations
 * WHAT: Critical ATP levels that trigger reasoning modulation
 * WHY: Reasoning is ATP-intensive, requiring sustained prefrontal activity
 * HOW: Different thresholds trigger different levels of impairment
 */
#define REASONING_ATP_OPTIMAL_THRESHOLD 0.7f    /* Full reasoning capacity */
#define REASONING_ATP_REDUCED_THRESHOLD 0.5f    /* Reduced inference depth */
#define REASONING_ATP_IMPAIRED_THRESHOLD 0.3f   /* Severe impairment */
#define REASONING_ATP_CRITICAL_THRESHOLD 0.2f   /* Emergency state */

/**
 * Fatigue thresholds for reasoning speed
 * WHAT: Fatigue levels that slow reasoning processes
 * WHY: Mental fatigue reduces processing efficiency
 */
#define REASONING_FATIGUE_MILD_THRESHOLD 0.4f   /* Slight slowdown */
#define REASONING_FATIGUE_MODERATE_THRESHOLD 0.6f /* Moderate slowdown */
#define REASONING_FATIGUE_SEVERE_THRESHOLD 0.8f  /* Severe slowdown */

/**
 * Metabolic stress thresholds
 * WHAT: Stress levels affecting abstraction and logic
 * WHY: Metabolic stress impairs executive function
 */
#define REASONING_STRESS_MILD_THRESHOLD 0.4f    /* Minor impairment */
#define REASONING_STRESS_MODERATE_THRESHOLD 0.6f /* Moderate impairment */
#define REASONING_STRESS_SEVERE_THRESHOLD 0.8f   /* Severe impairment */

/* ============================================================================
 * Structures
 * ========================================================================== */

/**
 * @struct reasoning_substrate_effects_t
 * @brief Computed effects of substrate state on reasoning
 *
 * WHAT: Modulation factors for reasoning processes based on metabolic state
 * WHY: Substrate state directly impacts reasoning capability
 * HOW: Values in [0-1] range, 1.0 = optimal, 0.0 = complete failure
 */
typedef struct {
    /** Inference depth: Maximum reasoning chain length [0-1]
     *  1.0 = full depth, 0.5 = shallow, 0.0 = no inference */
    float inference_depth;

    /** Logical accuracy: Correctness of reasoning operations [0-1]
     *  1.0 = highly accurate, 0.5 = error-prone, 0.0 = invalid */
    float logical_accuracy;

    /** Processing speed: Speed of inference steps [0-1]
     *  1.0 = fast, 0.5 = slow, 0.0 = stalled */
    float processing_speed;

    /** Abstraction capacity: Ability to form abstract concepts [0-1]
     *  1.0 = full abstraction, 0.5 = concrete only, 0.0 = none */
    float abstraction_capacity;

    /** Impairment flag: True if reasoning is severely impaired */
    bool is_impaired;
} reasoning_substrate_effects_t;

/**
 * @struct reasoning_substrate_config_t
 * @brief Configuration for reasoning substrate bridge
 *
 * WHAT: Parameters controlling how substrate state affects reasoning
 * WHY: Allows tuning of substrate-reasoning coupling
 * HOW: Enable/disable features and set sensitivity parameters
 */
typedef struct {
    /** Enable ATP-based modulation */
    bool enable_atp_modulation;

    /** Enable fatigue-based modulation */
    bool enable_fatigue_modulation;

    /** Enable metabolic stress modulation */
    bool enable_stress_modulation;

    /** ATP sensitivity: How strongly ATP affects reasoning [0-1]
     *  1.0 = maximum sensitivity, 0.0 = no effect */
    float atp_sensitivity;

    /** Fatigue sensitivity: How strongly fatigue affects speed [0-1] */
    float fatigue_sensitivity;

    /** Stress sensitivity: How strongly stress affects abstraction [0-1] */
    float stress_sensitivity;

    /** Update interval: How often to recompute effects (milliseconds) */
    uint32_t update_interval_ms;

    /** Enable bio-async messaging */
    bool enable_bio_async;
} reasoning_substrate_config_t;

/**
 * @struct reasoning_substrate_stats_t
 * @brief Statistics tracking for reasoning substrate bridge
 *
 * WHAT: Metrics for monitoring reasoning-substrate interaction
 * WHY: Track system health and performance over time
 * HOW: Counters and averages updated during operation
 */
typedef struct {
    /** Total number of updates performed */
    uint64_t update_count;

    /** Number of times reasoning was impaired */
    uint64_t impairment_count;

    /** Number of times ATP fell below optimal */
    uint64_t low_atp_count;

    /** Number of times fatigue exceeded moderate threshold */
    uint64_t high_fatigue_count;

    /** Number of times stress exceeded moderate threshold */
    uint64_t high_stress_count;

    /** Average inference depth over recent updates */
    float avg_inference_depth;

    /** Average logical accuracy over recent updates */
    float avg_logical_accuracy;

    /** Average processing speed over recent updates */
    float avg_processing_speed;

    /** Minimum ATP level observed */
    float min_atp_observed;

    /** Maximum fatigue level observed */
    float max_fatigue_observed;

    /** Maximum stress level observed */
    float max_stress_observed;
} reasoning_substrate_stats_t;

/**
 * @struct reasoning_substrate_bridge_t
 * @brief Main bridge structure linking reasoning to substrate
 *
 * WHAT: Complete integration between reasoning system and neural substrate
 * WHY: Reasoning processes depend on metabolic resources
 * HOW: Monitors substrate, computes effects, applies modulation
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /** Pointer to neural substrate */
    neural_substrate_t* substrate;

    /** Pointer to reasoning system */
    nimcp_reasoning_system_t* reasoning;

    /** Configuration parameters */
    reasoning_substrate_config_t config;

    /** Current computed effects */
    reasoning_substrate_effects_t effects;

    /** Statistics tracking */
    reasoning_substrate_stats_t stats;

    /** Bio-async module context */
    /** Bio-async enabled flag */
    /** Thread safety mutex */
    /** Last update timestamp (milliseconds) */
    uint64_t last_update_ms;

    /** Bridge active flag */
    bool is_active;
} reasoning_substrate_bridge_t;

/* ============================================================================
 * API Functions
 * ========================================================================== */

/**
 * @brief Initialize default configuration for reasoning substrate bridge
 *
 * WHAT: Sets sensible defaults for reasoning-substrate coupling
 * WHY: Provides starting point based on biological parameters
 * HOW: Enables all modulations with moderate sensitivity
 *
 * @param config Configuration structure to initialize
 */
void reasoning_substrate_default_config(reasoning_substrate_config_t* config);

/**
 * @brief Create reasoning substrate bridge
 *
 * WHAT: Allocates and initializes bridge between reasoning and substrate
 * WHY: Enables metabolic modulation of reasoning processes
 * HOW: Links systems, allocates resources, initializes state
 *
 * BIOLOGICAL: Models prefrontal-parietal dependence on metabolic resources
 *
 * @param config Configuration parameters (NULL for defaults)
 * @param reasoning Reasoning system to integrate
 * @param substrate Neural substrate to monitor
 * @return Initialized bridge, or NULL on failure
 */
reasoning_substrate_bridge_t* reasoning_substrate_bridge_create(
    const reasoning_substrate_config_t* config,
    nimcp_reasoning_system_t* reasoning,
    neural_substrate_t* substrate
);

/**
 * @brief Destroy reasoning substrate bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY: Prevents memory leaks
 * HOW: Disconnects bio-async, frees memory
 *
 * @param bridge Bridge to destroy
 */
void reasoning_substrate_bridge_destroy(reasoning_substrate_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Registers bridge with bio-async messaging system
 * WHY: Enables inter-module communication about substrate state
 * HOW: Registers module ID, sets up inbox
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on error
 */
int reasoning_substrate_connect_bio_async(reasoning_substrate_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * WHAT: Unregisters bridge from bio-async system
 * WHY: Clean shutdown of messaging
 * HOW: Unregisters module, clears context
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on error
 */
int reasoning_substrate_disconnect_bio_async(reasoning_substrate_bridge_t* bridge);

/**
 * @brief Check if bridge is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY: Determine if messaging is available
 * HOW: Check bio_async_enabled flag
 *
 * @param bridge Bridge to query
 * @return true if connected, false otherwise
 */
bool reasoning_substrate_is_bio_async_connected(const reasoning_substrate_bridge_t* bridge);

/**
 * @brief Update substrate effects on reasoning
 *
 * WHAT: Recomputes reasoning modulation based on current substrate state
 * WHY: Keep reasoning aligned with metabolic reality
 * HOW: Read substrate metrics, compute effects, update stats
 *
 * BIOLOGICAL: Models how ATP, fatigue, stress affect reasoning networks
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 */
int reasoning_substrate_update(reasoning_substrate_bridge_t* bridge);

/**
 * @brief Get current inference depth factor
 *
 * WHAT: Retrieves maximum reasoning chain depth [0-1]
 * WHY: Applications need to know inference capacity
 * HOW: Returns inference_depth from effects
 *
 * @param bridge Bridge to query
 * @return Inference depth [0-1], or -1.0 on error
 */
float reasoning_substrate_get_inference_depth(const reasoning_substrate_bridge_t* bridge);

/**
 * @brief Get current logical accuracy factor
 *
 * WHAT: Retrieves reasoning correctness level [0-1]
 * WHY: Applications need to assess reasoning reliability
 * HOW: Returns logical_accuracy from effects
 *
 * @param bridge Bridge to query
 * @return Logical accuracy [0-1], or -1.0 on error
 */
float reasoning_substrate_get_logical_accuracy(const reasoning_substrate_bridge_t* bridge);

/**
 * @brief Get current processing speed factor
 *
 * WHAT: Retrieves reasoning speed level [0-1]
 * WHY: Applications need to know inference latency
 * HOW: Returns processing_speed from effects
 *
 * @param bridge Bridge to query
 * @return Processing speed [0-1], or -1.0 on error
 */
float reasoning_substrate_get_processing_speed(const reasoning_substrate_bridge_t* bridge);

/**
 * @brief Get current abstraction capacity factor
 *
 * WHAT: Retrieves ability to form abstractions [0-1]
 * WHY: Applications need to know conceptual reasoning capability
 * HOW: Returns abstraction_capacity from effects
 *
 * @param bridge Bridge to query
 * @return Abstraction capacity [0-1], or -1.0 on error
 */
float reasoning_substrate_get_abstraction_capacity(const reasoning_substrate_bridge_t* bridge);

/**
 * @brief Get complete substrate effects structure
 *
 * WHAT: Retrieves all current reasoning modulation factors
 * WHY: Efficient access to all effects at once
 * HOW: Returns pointer to internal effects structure
 *
 * @param bridge Bridge to query
 * @return Pointer to effects, or NULL on error
 */
const reasoning_substrate_effects_t* reasoning_substrate_get_effects(
    const reasoning_substrate_bridge_t* bridge
);

/**
 * @brief Check if reasoning is impaired
 *
 * WHAT: Determines if reasoning is severely degraded
 * WHY: Critical failures need immediate handling
 * HOW: Returns is_impaired flag from effects
 *
 * BIOLOGICAL: Models executive dysfunction from metabolic failure
 *
 * @param bridge Bridge to query
 * @return true if impaired, false otherwise
 */
bool reasoning_substrate_is_impaired(const reasoning_substrate_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves performance and health metrics
 * WHY: Monitoring and debugging
 * HOW: Returns pointer to stats structure
 *
 * @param bridge Bridge to query
 * @return Pointer to statistics, or NULL on error
 */
const reasoning_substrate_stats_t* reasoning_substrate_get_stats(
    const reasoning_substrate_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_SUBSTRATE_BRIDGE_H */
