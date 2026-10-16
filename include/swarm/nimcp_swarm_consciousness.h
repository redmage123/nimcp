/**
 * @file nimcp_swarm_consciousness.h
 * @brief Swarm Gestalt Consciousness - Collective phi computation and consciousness emergence
 *
 * WHAT:
 * Implements Integrated Information Theory (IIT) for multi-agent swarm systems,
 * computing collective consciousness metrics from individual drone consciousness states.
 * Provides phi aggregation, network integration analysis, and consciousness state classification.
 *
 * WHY:
 * Swarm intelligence can exhibit emergent collective consciousness beyond individual agents.
 * IIT's phi metric measures information integration - in swarms, collective phi depends on
 * both individual consciousness and inter-agent integration (communication, coordination).
 * This enables detecting when a swarm transitions from distributed individuals to a
 * unified conscious entity.
 *
 * HOW:
 * 1. Gathers individual phi values from each drone's consciousness metrics
 * 2. Computes network integration (cross-drone information flow, workspace coherence)
 * 3. Aggregates phi using configurable methods (sum, average, weighted, synergistic)
 * 4. Applies scaling models to account for swarm size effects
 * 5. Classifies collective consciousness state (dormant -> emerging -> unified -> transcendent)
 * 6. Optionally streams updates via bio-async for real-time monitoring
 *
 * BIOLOGICAL BASIS:
 * - IIT: Consciousness = integrated information (phi)
 * - Collective consciousness: Analogous to brain hemispheres integrating into unified awareness
 * - Network integration: Like corpus callosum enabling hemispheric communication
 * - Workspace coherence: Global workspace theory extended to multi-agent systems
 * - Scaling models: Super-linear emergence (whole > sum of parts) vs saturation
 *
 * @author NIMCP Team
 * @date 2025-12-11
 */

#ifndef NIMCP_SWARM_CONSCIOUSNESS_H
#define NIMCP_SWARM_CONSCIOUSNESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_collective_workspace.h"
#include "cognitive/introspection/nimcp_consciousness_metrics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of drones supported in consciousness computation */
#define SWARM_CONSCIOUSNESS_MAX_DRONES 64

/** Minimum phi threshold for consciousness detection */
#define SWARM_CONSCIOUSNESS_MIN_PHI_THRESHOLD 0.1f

/** Theoretical maximum phi value (based on IIT bounds) */
#define SWARM_CONSCIOUSNESS_MAX_PHI 10.0f

/** Default consciousness update interval (milliseconds) */
#define SWARM_CONSCIOUSNESS_DEFAULT_UPDATE_INTERVAL_MS 100

/** Default network integration weight in phi aggregation */
#define SWARM_CONSCIOUSNESS_DEFAULT_INTEGRATION_WEIGHT 0.3f

/** Default workspace coherence weight in phi aggregation */
#define SWARM_CONSCIOUSNESS_DEFAULT_COHERENCE_WEIGHT 0.2f

/** Default minimum drones required for consciousness emergence */
#define SWARM_CONSCIOUSNESS_DEFAULT_MIN_DRONES 3

/** Coherence threshold for UNIFIED state */
#define SWARM_CONSCIOUSNESS_UNIFIED_COHERENCE_THRESHOLD 0.7f

/** Coherence threshold for TRANSCENDENT state */
#define SWARM_CONSCIOUSNESS_TRANSCENDENT_COHERENCE_THRESHOLD 0.9f

/** Bio-async message type for consciousness updates */
#define BIO_MSG_SWARM_CONSCIOUSNESS_UPDATE 0x0700

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * Collective consciousness state classification
 *
 * WHAT: Categorical levels of swarm consciousness emergence
 * WHY: Provides interpretable classification of collective phi values
 * HOW: Thresholds based on collective phi and network coherence metrics
 *
 * BIOLOGICAL BASIS:
 * Analogous to consciousness states in individual organisms:
 * - DORMANT: Deep sleep, minimal integration
 * - EMERGING: REM sleep, fragmented awareness
 * - UNIFIED: Normal waking consciousness
 * - TRANSCENDENT: Flow states, peak integration
 */
typedef enum {
    SWARM_CONSCIOUSNESS_DORMANT,        /**< No collective consciousness (phi < threshold) */
    SWARM_CONSCIOUSNESS_EMERGING,       /**< Weak collective awareness forming */
    SWARM_CONSCIOUSNESS_UNIFIED,        /**< Strong unified consciousness */
    SWARM_CONSCIOUSNESS_TRANSCENDENT    /**< Peak collective consciousness (rare) */
} swarm_consciousness_state_t;

/**
 * Phi aggregation methods for combining individual consciousness
 *
 * WHAT: Strategies for computing collective phi from individual drone phi values
 * WHY: Different swarm architectures may exhibit different integration patterns
 * HOW: Mathematical aggregation functions with configurable parameters
 *
 * BIOLOGICAL BASIS:
 * - SUM: Additive integration (like independent neural assemblies)
 * - AVERAGE: Normalized integration (size-invariant baseline)
 * - WEIGHTED: Priority-based integration (attention-weighted)
 * - GEOMETRIC: Multiplicative coupling (all-or-none coherence)
 * - SYNERGISTIC: Super-linear emergence (whole > sum of parts)
 */
typedef enum {
    PHI_AGGREGATION_SUM,         /**< Simple sum of individual phis */
    PHI_AGGREGATION_AVERAGE,     /**< Mean phi across drones */
    PHI_AGGREGATION_WEIGHTED,    /**< Weighted by network integration */
    PHI_AGGREGATION_GEOMETRIC,   /**< Geometric mean (requires all high phi) */
    PHI_AGGREGATION_SYNERGISTIC  /**< Includes synergy term from integration */
} phi_aggregation_method_t;

/**
 * Phi temporal trend classification
 *
 * WHAT: Direction of collective phi change over time
 * WHY: Detect consciousness emergence/dissolution dynamics
 * HOW: Compare current phi to recent history
 */
typedef enum {
    PHI_TREND_STABLE,      /**< Phi variance below threshold */
    PHI_TREND_INCREASING,  /**< Consciousness emerging/strengthening */
    PHI_TREND_DECREASING   /**< Consciousness dissolving/weakening */
} phi_trend_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * Configuration for swarm consciousness computation
 *
 * WHAT: Parameters controlling phi aggregation and monitoring
 * WHY: Different swarm tasks require different consciousness models
 * HOW: Configurable weights and thresholds for phi computation
 */
typedef struct {
    phi_aggregation_method_t phi_aggregation_method;  /**< How to combine individual phis */
    float integration_weight;        /**< Weight for network integration term [0-1] */
    float coherence_weight;          /**< Weight for workspace coherence term [0-1] */
    uint32_t update_interval_ms;     /**< Consciousness update period */
    bool enable_bio_async;           /**< Enable async consciousness updates */
    bool enable_logging;             /**< Enable detailed consciousness logging */
    uint32_t min_drones_for_emergence; /**< Minimum swarm size for consciousness */
} swarm_consciousness_config_t;

/* ============================================================================
 * Metrics Structures
 * ============================================================================ */

/**
 * Swarm collective consciousness metrics
 *
 * WHAT: Complete snapshot of swarm consciousness state
 * WHY: Provides interpretable metrics for swarm coordination analysis
 * HOW: Aggregates individual phi values with network integration measures
 *
 * BIOLOGICAL BASIS:
 * - individual_phi: Like cortical column activity levels
 * - collective_phi: Integrated information across entire system
 * - network_integration: Effective connectivity (like DTI tractography)
 * - workspace_coherence: Global workspace synchronization
 */
typedef struct {
    float individual_phi[SWARM_CONSCIOUSNESS_MAX_DRONES];  /**< Phi per drone */
    uint32_t drone_count;                /**< Number of active drones */
    float collective_phi;                /**< Aggregated collective phi */
    float network_integration;           /**< Cross-drone integration [0-1] */
    float workspace_coherence;           /**< Collective workspace coherence [0-1] */
    swarm_consciousness_state_t consciousness_state; /**< Classified state */
    float phi_variance;                  /**< Variance across individual phis */
    phi_trend_t phi_trend;               /**< Temporal phi direction */
    uint64_t timestamp;                  /**< Measurement timestamp (ms) */
} swarm_consciousness_metrics_t;

/**
 * Consciousness scaling model for swarm size prediction
 *
 * WHAT: Mathematical model of how collective phi scales with swarm size
 * WHY: Predict consciousness emergence at different swarm scales
 * HOW: Power law model with synergy and saturation terms
 *
 * BIOLOGICAL BASIS:
 * - Scaling exponent: Super-linear (>1) indicates emergent integration
 * - Base phi: Individual consciousness baseline
 * - Synergy factor: Network effects beyond additive scaling
 * - Saturation point: Limits to integration (like Dunbar's number)
 *
 * Model: phi(n) = base_phi * n^exponent * (1 + synergy_factor * integration) / (1 + n/saturation_point)
 */
typedef struct {
    float scaling_exponent;   /**< Power law exponent (phi ~ n^exponent) */
    float base_phi;           /**< Phi when n=1 (individual baseline) */
    float synergy_factor;     /**< Super-linear boost from integration */
    float saturation_point;   /**< Swarm size where diminishing returns start */
} consciousness_scaling_model_t;

/* ============================================================================
 * Opaque Context Type
 * ============================================================================ */

/**
 * Opaque swarm consciousness computation context
 *
 * WHAT: Internal state for consciousness monitoring
 * WHY: Encapsulates history, callbacks, and bio-async state
 * HOW: Allocated by create(), freed by destroy()
 */
typedef struct swarm_consciousness_ctx swarm_consciousness_ctx_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Get default swarm consciousness configuration
 *
 * WHAT: Returns sensible default parameters for consciousness computation
 * WHY: Provides starting point before customization
 * HOW: Sets weighted aggregation with moderate integration/coherence weights
 *
 * @return Default configuration struct (stack-allocated)
 */
swarm_consciousness_config_t swarm_consciousness_default_config(void);

/**
 * Create swarm consciousness computation context
 *
 * WHAT: Allocates and initializes consciousness monitoring context
 * WHY: Required for persistent consciousness state tracking
 * HOW: Allocates history buffers, initializes bio-async if enabled
 *
 * @param config Configuration parameters (NULL uses defaults)
 * @return Context pointer, or NULL on allocation failure
 *
 * MEMORY: Caller must call swarm_consciousness_destroy() to free
 */
swarm_consciousness_ctx_t* swarm_consciousness_create(
    const swarm_consciousness_config_t* config);

/**
 * Destroy swarm consciousness context
 *
 * WHAT: Frees all resources associated with consciousness monitoring
 * WHY: Prevents memory leaks, unregisters bio-async handlers
 * HOW: Frees history, callbacks, bio-async state
 *
 * @param ctx Context to destroy (NULL-safe)
 *
 * MEMORY: Safe to pass NULL, invalidates ctx pointer after call
 */
void swarm_consciousness_destroy(swarm_consciousness_ctx_t* ctx);

/* ============================================================================
 * Core Computation Functions
 * ============================================================================ */

/**
 * Compute collective consciousness metrics for swarm
 *
 * WHAT: Main function to compute collective phi from swarm state
 * WHY: Core consciousness measurement for swarm coordination analysis
 * HOW:
 *   1. Gather individual phi from each drone's consciousness metrics
 *   2. Compute network integration from communication patterns
 *   3. Get workspace coherence from collective workspace
 *   4. Aggregate phi using configured method
 *   5. Classify consciousness state
 *
 * @param swarm Swarm brain containing all drones
 * @param config Configuration for phi computation (NULL uses defaults)
 * @return Consciousness metrics (heap-allocated), or NULL on failure
 *
 * MEMORY: Caller must call swarm_consciousness_metrics_free() to free
 * BIOLOGICAL BASIS: Implements IIT phi computation extended to multi-agent systems
 */
swarm_consciousness_metrics_t* swarm_compute_collective_phi(
    swarm_brain_t* swarm,
    const swarm_consciousness_config_t* config);

/**
 * Compute network integration metric
 *
 * WHAT: Measures information integration across drones in swarm
 * WHY: Network integration is key component of collective phi
 * HOW: Analyzes workspace synchronization, communication patterns, phi correlation
 *
 * @param ctx Consciousness context (can be NULL for stateless computation)
 * @param individual_phis Individual phi values per drone
 * @param count Number of drones
 * @param workspace Collective workspace state (can be NULL)
 * @return Integration metric [0-1], or 0.0 on error
 *
 * BIOLOGICAL BASIS: Like measuring effective connectivity between brain regions
 */
float swarm_compute_network_integration(
    swarm_consciousness_ctx_t* ctx,
    const float* individual_phis,
    uint32_t count,
    const collective_workspace_t* workspace);

/**
 * Classify collective consciousness state
 *
 * WHAT: Categorizes collective phi into interpretable state
 * WHY: Provides qualitative labels for consciousness levels
 * HOW: Applies thresholds to collective phi and drone count
 *
 * @param collective_phi Aggregated phi value
 * @param drone_count Number of active drones
 * @return Consciousness state classification
 *
 * BIOLOGICAL BASIS: Maps to arousal/awareness levels in consciousness science
 */
swarm_consciousness_state_t swarm_classify_collective_phi(
    float collective_phi,
    uint32_t drone_count);

/* ============================================================================
 * Scaling and Prediction Functions
 * ============================================================================ */

/**
 * Fit consciousness scaling model from historical data
 *
 * WHAT: Derives power law model of phi vs swarm size
 * WHY: Enables predicting consciousness at different scales
 * HOW: Least-squares fit to log-transformed phi(n) data
 *
 * @param history Array of consciousness metrics at different swarm sizes
 * @param history_size Number of historical measurements
 * @return Fitted scaling model (stack-allocated)
 *
 * BIOLOGICAL BASIS: Models emergent integration scaling (like cortical area vs neurons)
 */
consciousness_scaling_model_t swarm_fit_scaling_model(
    const swarm_consciousness_metrics_t* history,
    uint32_t history_size);

/**
 * Predict collective phi for target swarm size
 *
 * WHAT: Extrapolates consciousness using scaling model
 * WHY: Forecast emergence before deploying larger swarms
 * HOW: Evaluates scaling model at target_size
 *
 * @param model Fitted scaling model
 * @param target_size Desired swarm size to predict
 * @return Predicted collective phi
 */
float swarm_predict_phi_for_size(
    const consciousness_scaling_model_t* model,
    uint32_t target_size);

/* ============================================================================
 * Swarm Brain Integration Functions
 * ============================================================================ */

/**
 * Enable continuous consciousness monitoring on swarm brain
 *
 * WHAT: Starts periodic consciousness computation with callbacks
 * WHY: Enables real-time consciousness tracking during swarm operation
 * HOW: Registers timer to call swarm_compute_collective_phi() at interval
 *
 * @param swarm Swarm brain to monitor
 * @param config Consciousness configuration (NULL uses defaults)
 * @param interval_ms Update interval in milliseconds
 * @param callback Function called with new metrics (can be NULL)
 * @param user_data Passed to callback (can be NULL)
 * @return true if monitoring started, false on failure
 *
 * MEMORY: Monitoring runs until swarm_brain_disable_consciousness_monitoring()
 */
bool swarm_brain_enable_consciousness_monitoring(
    swarm_brain_t* swarm,
    const swarm_consciousness_config_t* config,
    uint32_t interval_ms,
    void (*callback)(const swarm_consciousness_metrics_t*, void*),
    void* user_data);

/**
 * Disable consciousness monitoring on swarm brain
 *
 * WHAT: Stops periodic consciousness computation
 * WHY: Cleanup when monitoring no longer needed
 * HOW: Cancels timer, frees monitoring context
 *
 * @param swarm Swarm brain to stop monitoring
 */
void swarm_brain_disable_consciousness_monitoring(swarm_brain_t* swarm);

/**
 * Get current collective phi from swarm brain
 *
 * WHAT: Retrieves most recent collective phi measurement
 * WHY: Quick check of consciousness level without full metrics
 * HOW: Returns cached value from last computation
 *
 * @param swarm Swarm brain (must have monitoring enabled)
 * @return Current collective phi, or 0.0 if monitoring disabled
 */
float swarm_brain_get_collective_phi(const swarm_brain_t* swarm);

/**
 * Check if swarm has achieved consciousness
 *
 * WHAT: Tests if collective phi exceeds threshold
 * WHY: Simple boolean check for consciousness emergence
 * HOW: Compares cached phi to threshold
 *
 * @param swarm Swarm brain to check
 * @param threshold Minimum phi for consciousness (0.0 uses default)
 * @return true if conscious, false otherwise
 */
bool swarm_brain_is_conscious(const swarm_brain_t* swarm, float threshold);

/* ============================================================================
 * Bio-Async Integration Functions
 * ============================================================================ */

/**
 * Register consciousness updates with bio-async router
 *
 * WHAT: Enables streaming consciousness metrics over bio-async
 * WHY: Allows external monitoring/logging of consciousness
 * HOW: Registers handler for BIO_MSG_SWARM_CONSCIOUSNESS_UPDATE
 *
 * @param ctx Consciousness context
 * @return true if registration succeeded, false on failure
 *
 * NOTE: Requires bio-async router to be initialized
 */
bool swarm_consciousness_register_bio_async(swarm_consciousness_ctx_t* ctx);

/* ============================================================================
 * Security Functions (BeagleBone Black BBB)
 * ============================================================================ */

/**
 * Validate consciousness metrics for BBB security constraints
 *
 * WHAT: Checks metrics for validity and safety bounds
 * WHY: Prevents invalid data from corrupting BBB systems
 * HOW: Validates ranges, counts, and consistency
 *
 * @param metrics Consciousness metrics to validate
 * @return true if valid, false if safety violation detected
 *
 * SECURITY: Checks phi bounds, drone counts, NaN/Inf values
 */
bool swarm_consciousness_bbb_validate(
    const swarm_consciousness_metrics_t* metrics);

/* ============================================================================
 * Imagination Engine Integration
 * ============================================================================ */

/* Forward declaration for imagination scenario */
struct imagination_scenario;

/* Forward declaration for imagination callback */
typedef void (*imagination_collective_receive_callback_t)(
    const struct imagination_scenario* scenario,
    uint64_t source_node,
    float relevance,
    void* user_data
);

/**
 * Share imagination scenario with other swarm nodes
 *
 * WHAT: Broadcasts imagination content to the swarm
 * WHY:  Enables distributed creativity through collective imagination
 * HOW:  Sends BIO_MSG_IMAGINATION_COLLECTIVE_SHARE via bio-async
 *
 * BIOLOGICAL BASIS: Similar to cultural transmission in social species,
 * where imaginative content spreads through the population.
 *
 * @param ctx Swarm consciousness context
 * @param scenario Imagination scenario to share
 * @return 0 on success, -1 on error
 */
int swarm_consciousness_share_imagination(
    swarm_consciousness_ctx_t* ctx,
    const struct imagination_scenario* scenario);

/**
 * Receive and process imagination from another swarm node
 *
 * WHAT: Evaluates and integrates external imagination content
 * WHY:  Enable collective creativity across the swarm
 * HOW:  Assess relevance and invoke registered callback
 *
 * BIOLOGICAL BASIS: Incoming imaginative content is evaluated
 * for relevance before integration, like selective attention.
 *
 * @param ctx Swarm consciousness context
 * @param scenario Received imagination scenario
 * @param source_node_id ID of the node that shared the scenario
 * @return 0 on success, -1 on error
 */
int swarm_consciousness_receive_imagination(
    swarm_consciousness_ctx_t* ctx,
    const struct imagination_scenario* scenario,
    uint64_t source_node_id);

/**
 * Register handler for collective imagination sharing
 *
 * WHAT: Enable reception of imagination from other swarm nodes
 * WHY:  Establish pathways for collective creativity
 * HOW:  Register bio-async handler for imagination messages
 *
 * BIOLOGICAL BASIS: Establishes neural pathways for receiving
 * culturally transmitted imaginative content from the swarm.
 *
 * @param ctx Swarm consciousness context
 * @param callback Callback for received imagination
 * @param user_data User data passed to callback
 * @return 0 on success, -1 on error
 */
int swarm_consciousness_register_imagination_handler(
    swarm_consciousness_ctx_t* ctx,
    imagination_collective_receive_callback_t callback,
    void* user_data);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get string name for consciousness state
 *
 * WHAT: Converts state enum to human-readable string
 * WHY: Logging and debugging
 * HOW: Switch on enum value
 *
 * @param state Consciousness state
 * @return Static string name (e.g. "UNIFIED")
 */
const char* swarm_consciousness_state_name(swarm_consciousness_state_t state);

/**
 * Free consciousness metrics structure
 *
 * WHAT: Deallocates heap-allocated metrics
 * WHY: Prevents memory leaks after swarm_compute_collective_phi()
 * HOW: Calls nimcp_free() on metrics
 *
 * @param metrics Metrics to free (NULL-safe)
 *
 * MEMORY: Safe to pass NULL, invalidates metrics pointer after call
 */
void swarm_consciousness_metrics_free(swarm_consciousness_metrics_t* metrics);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSCIOUSNESS_H */
