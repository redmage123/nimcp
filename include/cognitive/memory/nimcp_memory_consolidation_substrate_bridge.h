/**
 * @file nimcp_memory_consolidation_substrate_bridge.h
 * @brief Neural substrate integration for memory consolidation module
 *
 * WHAT: Bidirectional bridge between memory consolidation and neural substrate
 * WHY: Memory consolidation requires substantial ATP for protein synthesis,
 *      hippocampal replay, and synaptic remodeling. Metabolic stress impairs
 *      consolidation rate, especially during sleep-dependent consolidation.
 * HOW: Monitor substrate state (ATP, metabolic stress), modulate consolidation
 *      rate, protein synthesis, replay efficiency, and hippocampal→cortical transfer
 *
 * BIOLOGICAL BASIS:
 * - Memory consolidation requires protein synthesis (ATP-intensive process)
 * - Hippocampal replay during sleep consumes metabolic energy
 * - Systems consolidation (hippocampus→cortex) requires sustained ATP
 * - Metabolic stress during sleep impairs consolidation quality
 * - LTP maintenance depends on protein synthesis (ATP-dependent)
 * - Synaptic tagging and capture mechanism requires local protein production
 *
 * SUBSTRATE EFFECTS:
 * - ATP depletion → reduced protein synthesis → impaired LTP consolidation
 * - Metabolic stress → slower consolidation rate → incomplete memory stabilization
 * - Hypoxia → reduced replay efficiency → poor pattern completion
 * - Energy crisis → impaired hippocampal→cortical transfer
 */

#ifndef NIMCP_MEMORY_CONSOLIDATION_SUBSTRATE_BRIDGE_H
#define NIMCP_MEMORY_CONSOLIDATION_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for memory consolidation system (opaque pointer) */
typedef struct memory_consolidation memory_consolidation_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * Bio-async module ID for memory consolidation substrate bridge
 * Range: 0x1200-0x12FF (substrate bridges)
 */
#define BIO_MODULE_SUBSTRATE_MEMORY_CONSOLIDATION 0x1205

/**
 * ATP thresholds for consolidation (very ATP-intensive process)
 * Memory consolidation requires more ATP than most cognitive processes
 * due to protein synthesis requirements
 */
#define CONSOLIDATION_SUBSTRATE_ATP_CRITICAL    0.20f  /**< Critical ATP threshold (severe impairment) */
#define CONSOLIDATION_SUBSTRATE_ATP_LOW         0.40f  /**< Low ATP threshold (moderate impairment) */
#define CONSOLIDATION_SUBSTRATE_ATP_MODERATE    0.60f  /**< Moderate ATP threshold (mild impairment) */
#define CONSOLIDATION_SUBSTRATE_ATP_OPTIMAL     0.80f  /**< Optimal ATP threshold (full consolidation) */

/**
 * Metabolic stress thresholds for consolidation
 */
#define CONSOLIDATION_SUBSTRATE_STRESS_SEVERE   0.70f  /**< Severe stress (halt consolidation) */
#define CONSOLIDATION_SUBSTRATE_STRESS_HIGH     0.50f  /**< High stress (slow consolidation) */
#define CONSOLIDATION_SUBSTRATE_STRESS_MODERATE 0.30f  /**< Moderate stress (slightly reduced) */

/**
 * Default sensitivity parameters
 */
#define CONSOLIDATION_SUBSTRATE_DEFAULT_ATP_SENSITIVITY     1.0f  /**< ATP impact on consolidation */
#define CONSOLIDATION_SUBSTRATE_DEFAULT_STRESS_SENSITIVITY  0.8f  /**< Metabolic stress impact */
#define CONSOLIDATION_SUBSTRATE_DEFAULT_HYPOXIA_SENSITIVITY 0.9f  /**< Hypoxia impact on replay */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * Consolidation substrate effects
 *
 * WHAT: Computed effects of neural substrate on consolidation processes
 * WHY: Quantify how ATP, metabolic stress, and hypoxia affect memory consolidation
 * HOW: Continuous values representing impact on different consolidation mechanisms
 */
typedef struct {
    float consolidation_rate;      /**< Overall consolidation speed [0-1], 1=full speed */
    float protein_synthesis_rate;  /**< LTP-dependent protein synthesis rate [0-1] */
    float replay_efficiency;       /**< Hippocampal replay efficiency [0-1] */
    float transfer_rate;           /**< Hippocampal→cortical transfer rate [0-1] */
    bool is_impaired;              /**< True if substrate critically impairs consolidation */
} consolidation_substrate_effects_t;

/**
 * Consolidation substrate bridge configuration
 *
 * WHAT: Configuration for substrate-consolidation integration
 * WHY: Allow tuning of substrate effects on consolidation mechanisms
 * HOW: Feature flags and sensitivity parameters
 */
typedef struct {
    /* Feature enables */
    bool enable_atp_modulation;       /**< Enable ATP modulation of consolidation */
    bool enable_stress_modulation;    /**< Enable metabolic stress modulation */
    bool enable_hypoxia_modulation;   /**< Enable hypoxia modulation of replay */
    bool enable_protein_synthesis;    /**< Enable protein synthesis tracking */

    /* Sensitivity parameters */
    float atp_sensitivity;            /**< Sensitivity to ATP depletion [0-2] */
    float stress_sensitivity;         /**< Sensitivity to metabolic stress [0-2] */
    float hypoxia_sensitivity;        /**< Sensitivity to hypoxia [0-2] */

    /* Update parameters */
    uint32_t update_interval_ms;      /**< How often to update effects (ms) */
    bool auto_update;                 /**< Automatically update on substrate changes */
} consolidation_substrate_config_t;

/**
 * Consolidation substrate bridge statistics
 *
 * WHAT: Tracking statistics for substrate-consolidation interaction
 * WHY: Monitor impact of substrate on consolidation over time
 * HOW: Accumulate counts and track min/max values
 */
typedef struct {
    /* Update tracking */
    uint64_t update_count;            /**< Number of substrate updates processed */
    uint64_t impairment_count;        /**< Number of times consolidation was impaired */

    /* Effect statistics */
    float min_consolidation_rate;     /**< Minimum consolidation rate observed */
    float max_consolidation_rate;     /**< Maximum consolidation rate observed */
    float avg_consolidation_rate;     /**< Average consolidation rate */

    float min_protein_synthesis;      /**< Minimum protein synthesis rate */
    float max_protein_synthesis;      /**< Maximum protein synthesis rate */
    float avg_protein_synthesis;      /**< Average protein synthesis rate */

    float min_replay_efficiency;      /**< Minimum replay efficiency */
    float max_replay_efficiency;      /**< Maximum replay efficiency */
    float avg_replay_efficiency;      /**< Average replay efficiency */

    /* Substrate state statistics */
    float min_atp_observed;           /**< Minimum ATP level observed */
    float max_stress_observed;        /**< Maximum metabolic stress observed */

    /* Timing */
    uint64_t last_update_time_ms;     /**< Last update timestamp (ms) */
    uint64_t total_impaired_time_ms;  /**< Total time in impaired state (ms) */
} consolidation_substrate_stats_t;

/**
 * Memory consolidation substrate bridge
 *
 * WHAT: Complete integration bridge between consolidation module and neural substrate
 * WHY: Coordinate bidirectional effects between metabolic state and memory consolidation
 * HOW: Monitor substrate, compute effects, update consolidation module
 */
typedef struct consolidation_substrate_bridge_t {
    bridge_base_t base;                  /**< MUST be first: base bridge infrastructure */

    /* Core components */
    neural_substrate_t* substrate;                /**< Neural substrate instance */
    memory_consolidation_t* consolidation;        /**< Memory consolidation module */

    /* Configuration and state */
    consolidation_substrate_config_t config;      /**< Bridge configuration */
    consolidation_substrate_effects_t effects;    /**< Current substrate effects */
    consolidation_substrate_stats_t stats;        /**< Bridge statistics */

} consolidation_substrate_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Get default consolidation substrate bridge configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY: Provide baseline configuration for typical use cases
 * HOW: Set feature enables, sensitivity parameters, and update intervals
 *
 * @param config Configuration structure to initialize
 */
void consolidation_substrate_default_config(consolidation_substrate_config_t* config);

/**
 * Create memory consolidation substrate bridge
 *
 * WHAT: Initialize bridge between consolidation module and neural substrate
 * WHY: Enable substrate state to modulate memory consolidation processes
 * HOW: Allocate bridge, link components, initialize state
 *
 * BIOLOGICAL BASIS: Memory consolidation is metabolically expensive, requiring
 * sustained ATP for protein synthesis, hippocampal replay, and synaptic remodeling.
 *
 * @param config Bridge configuration
 * @param consolidation Memory consolidation module instance
 * @param substrate Neural substrate instance
 * @return Bridge instance, or NULL on failure
 */
consolidation_substrate_bridge_t* consolidation_substrate_bridge_create(
    const consolidation_substrate_config_t* config,
    memory_consolidation_t* consolidation,
    neural_substrate_t* substrate
);

/**
 * Destroy memory consolidation substrate bridge
 *
 * WHAT: Clean up bridge resources
 * WHY: Prevent memory leaks and dangling references
 * HOW: Disconnect bio-async, destroy mutex, free memory
 *
 * @param bridge Bridge instance to destroy
 */
void consolidation_substrate_bridge_destroy(consolidation_substrate_bridge_t* bridge);

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

/**
 * Connect consolidation substrate bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module for inter-module messaging
 * WHY: Enable coordination with other substrate bridges and modules
 * HOW: Register with BIO_MODULE_SUBSTRATE_MEMORY_CONSOLIDATION ID
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int consolidation_substrate_connect_bio_async(consolidation_substrate_bridge_t* bridge);

/**
 * Disconnect consolidation substrate bridge from bio-async router
 *
 * WHAT: Unregister bridge from bio-async messaging
 * WHY: Clean shutdown of inter-module communication
 * HOW: Deregister module context
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int consolidation_substrate_disconnect_bio_async(consolidation_substrate_bridge_t* bridge);

/**
 * Check if consolidation substrate bridge is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY: Verify messaging capability before sending messages
 * HOW: Check bio_async_enabled flag
 *
 * @param bridge Bridge instance
 * @return true if connected, false otherwise
 */
bool consolidation_substrate_is_bio_async_connected(const consolidation_substrate_bridge_t* bridge);

/* ============================================================================
 * Update Functions
 * ============================================================================ */

/**
 * Update consolidation substrate effects
 *
 * WHAT: Recompute substrate effects on consolidation based on current state
 * WHY: Keep consolidation modulation synchronized with substrate changes
 * HOW: Read substrate state (ATP, stress, hypoxia), compute effects, update stats
 *
 * BIOLOGICAL BASIS:
 * - Low ATP → reduced protein synthesis → impaired LTP consolidation
 * - High metabolic stress → slower consolidation rate
 * - Hypoxia → reduced hippocampal replay efficiency
 * - Combined effects can halt consolidation entirely
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int consolidation_substrate_update(consolidation_substrate_bridge_t* bridge);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * Get current consolidation rate
 *
 * WHAT: Retrieve overall consolidation speed factor
 * WHY: Query how fast consolidation is proceeding given substrate state
 * HOW: Return consolidation_rate from effects structure [0-1]
 *
 * @param bridge Bridge instance
 * @return Consolidation rate [0-1], 1=full speed, 0=halted
 */
float consolidation_substrate_get_consolidation_rate(const consolidation_substrate_bridge_t* bridge);

/**
 * Get current protein synthesis rate
 *
 * WHAT: Retrieve LTP-dependent protein synthesis rate
 * WHY: Query capacity for stabilizing synaptic changes
 * HOW: Return protein_synthesis_rate from effects structure [0-1]
 *
 * BIOLOGICAL BASIS: LTP consolidation requires local protein synthesis
 * at tagged synapses (synaptic tagging and capture mechanism)
 *
 * @param bridge Bridge instance
 * @return Protein synthesis rate [0-1], 1=full rate
 */
float consolidation_substrate_get_protein_synthesis_rate(const consolidation_substrate_bridge_t* bridge);

/**
 * Get current replay efficiency
 *
 * WHAT: Retrieve hippocampal replay efficiency
 * WHY: Query quality of offline memory replay (critical for consolidation)
 * HOW: Return replay_efficiency from effects structure [0-1]
 *
 * BIOLOGICAL BASIS: Hippocampal replay during sleep reactivates memory
 * traces, enabling cortical integration. Requires metabolic energy.
 *
 * @param bridge Bridge instance
 * @return Replay efficiency [0-1], 1=full efficiency
 */
float consolidation_substrate_get_replay_efficiency(const consolidation_substrate_bridge_t* bridge);

/**
 * Get current transfer rate
 *
 * WHAT: Retrieve hippocampal→cortical transfer rate
 * WHY: Query speed of systems consolidation (hippocampus to neocortex)
 * HOW: Return transfer_rate from effects structure [0-1]
 *
 * BIOLOGICAL BASIS: Systems consolidation gradually transfers memories
 * from hippocampus to distributed cortical representations
 *
 * @param bridge Bridge instance
 * @return Transfer rate [0-1], 1=full transfer rate
 */
float consolidation_substrate_get_transfer_rate(const consolidation_substrate_bridge_t* bridge);

/**
 * Get current consolidation substrate effects
 *
 * WHAT: Retrieve complete effects structure
 * WHY: Access all substrate effects on consolidation simultaneously
 * HOW: Return copy of effects structure
 *
 * @param bridge Bridge instance
 * @return Effects structure (copy)
 */
consolidation_substrate_effects_t consolidation_substrate_get_effects(
    const consolidation_substrate_bridge_t* bridge
);

/**
 * Check if consolidation is critically impaired
 *
 * WHAT: Query whether substrate state critically impairs consolidation
 * WHY: Detect conditions requiring intervention or alternative strategies
 * HOW: Return is_impaired flag from effects structure
 *
 * @param bridge Bridge instance
 * @return true if critically impaired, false otherwise
 */
bool consolidation_substrate_is_impaired(const consolidation_substrate_bridge_t* bridge);

/**
 * Get consolidation substrate bridge statistics
 *
 * WHAT: Retrieve accumulated statistics
 * WHY: Monitor substrate impact on consolidation over time
 * HOW: Return copy of stats structure
 *
 * @param bridge Bridge instance
 * @return Statistics structure (copy)
 */
consolidation_substrate_stats_t consolidation_substrate_get_stats(
    const consolidation_substrate_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_CONSOLIDATION_SUBSTRATE_BRIDGE_H */
