//=============================================================================
// nimcp_pr_pink_noise_bridge.h - Pink Noise Bridge for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_pr_pink_noise_bridge.h
 * @brief Bridge between Prime Resonant memory system and pink noise (1/f) module
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Integrates pink noise (1/f) fluctuations into all Prime Resonant memory
 *       dynamics including consolidation timing, resonance scoring, decay rates,
 *       quaternion drift, entanglement weights, and retrieval latency
 * WHY:  Real biological memory systems exhibit ubiquitous 1/f noise patterns
 *       that prevent synchronization artifacts, enable serendipitous associations,
 *       add natural variability, and match observed neural dynamics
 * HOW:  Maintains multiple correlated pink noise generators targeting different
 *       memory subsystems, with fractal timing for consolidation events and
 *       correlated quaternionic noise for multi-dimensional state evolution
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   1/f Noise in Memory Systems:
 *   +-----------------------------------------------------------------------+
 *   |  Pink noise (1/f) appears throughout the brain and memory systems:   |
 *   |                                                                        |
 *   |  1. CONSOLIDATION TIMING:                                              |
 *   |     - Time between consolidation events follows 1/f distribution     |
 *   |     - Prevents synchronization artifacts                              |
 *   |     - Matches biological replay timing (Carr et al., 2011)           |
 *   |                                                                        |
 *   |  2. RESONANCE FLUCTUATIONS:                                            |
 *   |     - Add 1/f modulation to resonance scores                          |
 *   |     - Prevents static memory hierarchies                              |
 *   |     - Enables serendipitous associations                              |
 *   |                                                                        |
 *   |  3. DECAY RATE MODULATION:                                             |
 *   |     - Decay rates fluctuate with 1/f noise                            |
 *   |     - Some memories decay faster, some slower                         |
 *   |     - Natural variability in retention                                |
 *   |                                                                        |
 *   |  4. QUATERNION DRIFT:                                                  |
 *   |     - Small 1/f perturbations to quaternion state                     |
 *   |     - Prevents overfitting to exact patterns                          |
 *   |     - Adds biological realism                                         |
 *   |                                                                        |
 *   |  5. ENTANGLEMENT WEIGHT FLUCTUATIONS:                                  |
 *   |     - Edge weights vary with 1/f noise                                |
 *   |     - Enables exploration of association space                        |
 *   |     - Matches synaptic weight variability                             |
 *   |                                                                        |
 *   |  6. RETRIEVAL LATENCY:                                                 |
 *   |     - Response times exhibit 1/f characteristics                      |
 *   |     - Models reaction time variability                                |
 *   |     - Adds temporal realism to queries                                |
 *   +-----------------------------------------------------------------------+
 *
 *   Fractal Timing Model:
 *   +-----------------------------------------------------------------------+
 *   |  Event timing uses fractional Brownian motion (fBm):                  |
 *   |                                                                        |
 *   |  interval(t) = base_interval * exp(amplitude * pink_noise(t))        |
 *   |                                                                        |
 *   |  With Hurst exponent H controlling persistence:                       |
 *   |  - H = 0.5: Random walk (uncorrelated intervals)                      |
 *   |  - H > 0.5: Persistent (long intervals tend to cluster)              |
 *   |  - H < 0.5: Anti-persistent (alternating short/long)                 |
 *   |                                                                        |
 *   |  Typical brain: H ~ 0.7-0.8 (mildly persistent)                       |
 *   +-----------------------------------------------------------------------+
 *
 *   Correlated Quaternion Noise:
 *   +-----------------------------------------------------------------------+
 *   |  Quaternion components (w,x,y,z) evolve with correlated noise:        |
 *   |                                                                        |
 *   |  Generate independent noise: I = [i_w, i_x, i_y, i_z]                |
 *   |  Apply Cholesky transform: N = L * I where C = L * L^T               |
 *   |  Result: E[N_i * N_j] = C[i][j] (correlation preserved)              |
 *   |                                                                        |
 *   |  Default correlations:                                                |
 *   |  - w-x: -0.3 (consolidation inversely affects emotion)               |
 *   |  - w-y: +0.5 (consolidation increases salience)                      |
 *   |  - w-z: +0.7 (consolidated memories more accessible)                 |
 *   |  - x-y: +0.4 (emotional content more salient)                        |
 *   |  - x-z: +0.2 (emotional memories more accessible)                    |
 *   |  - y-z: +0.6 (salient memories more accessible)                      |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Create bridge: O(1) (allocates generators)
 * - Next consolidation time: O(1) (single noise sample)
 * - Modulate resonance: O(1) per score, O(n) for batch
 * - Quaternion drift: O(1) (4 noise samples + matrix mult)
 * - Modulate graph: O(E) where E = edges
 * - Fractal analysis: O(N log N) for N history samples
 *
 * MEMORY:
 * - pr_pink_bridge_t: ~2KB base + generator states
 * - Per generator: ~500 bytes
 * - History buffer: configurable (default 4096 samples)
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Generators maintain internal state (not shared across threads)
 * - Statistics use atomic operations
 *
 * INTEGRATION:
 * - Core: Uses nimcp_pink_noise.h and nimcp_pr_pink_noise.h
 * - Target: Z-Ladder, Entanglement Graph, Theta-Gamma
 * - Analysis: nimcp_fractal.h for validation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_PINK_NOISE_BRIDGE_H
#define NIMCP_PR_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Dependencies
#include "plasticity/noise/nimcp_pink_noise.h"
#include "cognitive/memory/core/nimcp_pr_pink_noise.h"
#include "cognitive/memory/core/nimcp_fractal.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_z_ladder.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Default base interval for consolidation events (milliseconds) */
#define PR_PINK_BRIDGE_DEFAULT_CONSOLIDATION_INTERVAL_MS  100.0f

/** Minimum consolidation interval (prevents event clustering) */
#define PR_PINK_BRIDGE_MIN_CONSOLIDATION_INTERVAL_MS      10.0f

/** Maximum consolidation interval (prevents starvation) */
#define PR_PINK_BRIDGE_MAX_CONSOLIDATION_INTERVAL_MS      10000.0f

/** Default resonance modulation amplitude */
#define PR_PINK_BRIDGE_DEFAULT_RESONANCE_AMPLITUDE        0.05f

/** Default decay modulation amplitude */
#define PR_PINK_BRIDGE_DEFAULT_DECAY_AMPLITUDE            0.1f

/** Default quaternion drift amplitude */
#define PR_PINK_BRIDGE_DEFAULT_QUAT_AMPLITUDE             0.02f

/** Default entanglement weight fluctuation amplitude */
#define PR_PINK_BRIDGE_DEFAULT_ENTANGLE_AMPLITUDE         0.05f

/** Default promotion timing amplitude */
#define PR_PINK_BRIDGE_DEFAULT_PROMOTION_AMPLITUDE        0.1f

/** Default retrieval latency amplitude */
#define PR_PINK_BRIDGE_DEFAULT_RETRIEVAL_AMPLITUDE        0.15f

/** Default spectral exponent (1.0 = true pink noise) */
#define PR_PINK_BRIDGE_DEFAULT_SPECTRAL_EXPONENT          1.0f

/** Default Hurst exponent for fractal timing */
#define PR_PINK_BRIDGE_DEFAULT_HURST_EXPONENT             0.75f

/** Default sample rate for generators (Hz) */
#define PR_PINK_BRIDGE_DEFAULT_SAMPLE_RATE_HZ             1000.0f

/** Default correlation time (seconds) */
#define PR_PINK_BRIDGE_DEFAULT_CORRELATION_TIME           1.0f

/** History buffer size for analysis */
#define PR_PINK_BRIDGE_DEFAULT_HISTORY_SIZE               4096

/** Tolerance for pink noise validation */
#define PR_PINK_BRIDGE_VALIDATION_TOLERANCE               0.15f

/** Maximum targets count */
#define PR_PINK_TARGET_MAX                                8

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Modulation targets for pink noise
 *
 * WHAT: Identifies which memory subsystem receives 1/f modulation
 * WHY:  Allow fine-grained control over which dynamics exhibit noise
 * HOW:  Enum values index into generator arrays
 */
typedef enum {
    PR_PINK_TARGET_CONSOLIDATION = 0,  /**< Consolidation timing intervals */
    PR_PINK_TARGET_RESONANCE,          /**< Resonance score fluctuation */
    PR_PINK_TARGET_DECAY,              /**< Decay rate modulation */
    PR_PINK_TARGET_QUATERNION,         /**< Quaternion state drift */
    PR_PINK_TARGET_ENTANGLEMENT,       /**< Edge weight fluctuation */
    PR_PINK_TARGET_PROMOTION,          /**< Promotion timing/eligibility */
    PR_PINK_TARGET_RETRIEVAL,          /**< Retrieval latency */
    PR_PINK_TARGET_ALL                 /**< All targets (for batch ops) */
} pr_pink_target_t;

/**
 * @brief Modulation parameters for a single target
 *
 * WHAT: Configuration for pink noise modulation of one subsystem
 * WHY:  Tune modulation characteristics per target
 * HOW:  Amplitude, correlation time, and spectral shape
 */
typedef struct {
    float amplitude;              /**< Modulation amplitude [0-1] */
    float correlation_time;       /**< Temporal correlation (seconds) */
    float spectral_exponent;      /**< Target exponent (1.0 = pink) */
    bool enabled;                 /**< Enable/disable this modulation */
} pr_pink_modulation_params_t;

/**
 * @brief Correlated generators for quaternion components
 *
 * WHAT: Four correlated pink noise streams for quaternion evolution
 * WHY:  Components should evolve coherently, not independently
 * HOW:  Cholesky decomposition of correlation matrix
 */
typedef struct {
    pr_quat_pink_state_t* quat_state;    /**< Quaternionic pink noise state */
    float correlation_matrix[4][4];       /**< Cross-correlation coefficients */
    float current_noise[4];               /**< Last generated noise vector */
} pr_quat_pink_generators_t;

/**
 * @brief Fractal timing state for event scheduling
 *
 * WHAT: Generates 1/f-distributed inter-event intervals
 * WHY:  Memory consolidation events should have fractal timing
 * HOW:  Pink noise modulates base interval exponentially
 */
typedef struct {
    pr_fractal_timing_t* timing;  /**< Fractal timing generator */
    float current_interval_ms;    /**< Current event interval */
    float base_interval_ms;       /**< Base interval (seconds) */
    float hurst_exponent;         /**< H for fBm (0.5-0.9) */
    uint64_t last_event_time_ms;  /**< Time of last event */
    uint64_t next_event_time_ms;  /**< Scheduled next event */
    uint64_t event_count;         /**< Total events generated */
} pr_fractal_timer_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Complete configuration for pink noise bridge
 * WHY:  Single struct for initialization
 * HOW:  Per-target params plus global settings
 */
typedef struct {
    pr_pink_modulation_params_t consolidation;  /**< Consolidation timing params */
    pr_pink_modulation_params_t resonance;      /**< Resonance modulation params */
    pr_pink_modulation_params_t decay;          /**< Decay rate params */
    pr_pink_modulation_params_t quaternion;     /**< Quaternion drift params */
    pr_pink_modulation_params_t entanglement;   /**< Entanglement weight params */
    pr_pink_modulation_params_t promotion;      /**< Promotion timing params */
    pr_pink_modulation_params_t retrieval;      /**< Retrieval latency params */
    float global_amplitude;                     /**< Master amplitude scale [0-1] */
    float quaternion_correlation[6];            /**< w-x, w-y, w-z, x-y, x-z, y-z */
    float sample_rate_hz;                       /**< Sample rate for generators */
    uint32_t seed;                              /**< Random seed (0 = time-based) */
    size_t history_size;                        /**< History buffer size */
} pr_pink_bridge_config_t;

/**
 * @brief Opaque bridge handle
 *
 * Internal structure contains:
 * - Multiple pink noise generators (one per target)
 * - Quaternion generator with correlation
 * - Fractal timing for consolidation
 * - History buffers for analysis
 * - Mutex for thread safety
 * - Statistics counters
 */
typedef struct pr_pink_bridge_struct* pr_pink_bridge_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for monitoring and debugging
 * WHY:  Track noise quality and resource usage
 */
typedef struct {
    /* Consolidation timing statistics */
    float mean_consolidation_interval_ms;  /**< Average interval between events */
    float consolidation_variability;       /**< Coefficient of variation */
    uint64_t consolidation_events;         /**< Total consolidation events */

    /* Modulation statistics */
    float mean_resonance_modulation;       /**< Average resonance adjustment */
    float mean_decay_modulation;           /**< Average decay adjustment */
    float mean_quaternion_drift;           /**< Average quaternion change */
    float mean_entanglement_modulation;    /**< Average edge weight change */

    /* Generator statistics */
    uint64_t total_noise_samples;          /**< Total samples generated */
    uint64_t total_modulations;            /**< Total modulation applications */

    /* Spectral quality */
    float measured_spectral_exponent;      /**< Measured alpha value */
    float spectral_fit_r2;                 /**< R^2 for 1/f fit */

    /* Resource usage */
    size_t memory_bytes;                   /**< Approximate memory usage */
    uint64_t mutex_contentions;            /**< Lock contention count */
} pr_pink_bridge_stats_t;

/**
 * @brief Memory dynamics history for analysis
 *
 * WHAT: Time series of memory system metrics
 * WHY:  Validate 1/f characteristics are being achieved
 */
typedef struct {
    float* intervals;             /**< Consolidation interval history */
    float* resonance_samples;     /**< Resonance modulation history */
    float* decay_samples;         /**< Decay modulation history */
    size_t sample_count;          /**< Samples in history */
    size_t max_samples;           /**< Maximum history size */
    size_t write_index;           /**< Circular buffer position */
} pr_pink_history_t;

/**
 * @brief Theta-gamma integration state
 *
 * WHAT: Coupling between pink noise and theta-gamma oscillations
 * WHY:  Coordinate 1/f noise with neural rhythm phases
 */
typedef struct {
    float theta_phase;            /**< Current theta phase [0, 2*pi] */
    float theta_frequency_hz;     /**< Theta oscillation frequency */
    float coupling_strength;      /**< Phase-amplitude coupling [0-1] */
    bool enabled;                 /**< Integration enabled */
} pr_pink_theta_coupling_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults for memory modulation
 * WHY:  Provides biologically-inspired starting point
 * HOW:  Based on neuroscience literature values
 *
 * DEFAULT VALUES:
 * - consolidation: amplitude=0.3, correlation_time=1.0s, alpha=1.0
 * - resonance: amplitude=0.05, correlation_time=0.5s, alpha=1.0
 * - decay: amplitude=0.1, correlation_time=2.0s, alpha=1.0
 * - quaternion: amplitude=0.02, correlation_time=1.0s, alpha=1.0
 * - entanglement: amplitude=0.05, correlation_time=1.0s, alpha=1.0
 * - promotion: amplitude=0.1, correlation_time=1.0s, alpha=1.0
 * - retrieval: amplitude=0.15, correlation_time=0.5s, alpha=1.0
 * - global_amplitude=1.0
 * - quaternion_correlation=[default PR values]
 * - sample_rate_hz=1000.0
 * - seed=0 (time-based)
 * - history_size=4096
 *
 * @return Default configuration
 */
NIMCP_EXPORT pr_pink_bridge_config_t pr_pink_bridge_default_config(void);

/**
 * @brief Get default modulation parameters for a target
 *
 * @param target Modulation target
 * @return Default parameters for that target
 */
NIMCP_EXPORT pr_pink_modulation_params_t pr_pink_bridge_default_params(
    pr_pink_target_t target);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Checks configuration values are in valid ranges
 * WHY:  Prevent invalid configurations causing runtime errors
 *
 * VALIDATION RULES:
 * - Amplitudes in [0, 1]
 * - Correlation times > 0
 * - Spectral exponents in [0, 3]
 * - Sample rate > 0
 * - Quaternion correlations form positive-definite matrix
 *
 * @param config Configuration to validate
 * @return true if valid, false if invalid
 */
NIMCP_EXPORT bool pr_pink_bridge_validate_config(
    const pr_pink_bridge_config_t* config);

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

/**
 * @brief Create pink noise bridge
 *
 * WHAT: Initializes bridge with all generators and buffers
 * WHY:  Entry point for pink noise integration
 * HOW:  Creates generators per target, initializes timing, allocates history
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(history_size) for buffer allocation
 * MEMORY: ~10KB + history buffers
 *
 * EXAMPLE:
 * @code
 * pr_pink_bridge_config_t config = pr_pink_bridge_default_config();
 * config.consolidation.amplitude = 0.4f;  // More variable timing
 * pr_pink_bridge_t bridge = pr_pink_bridge_create(&config);
 * @endcode
 */
NIMCP_EXPORT pr_pink_bridge_t pr_pink_bridge_create(
    const pr_pink_bridge_config_t* config);

/**
 * @brief Destroy pink noise bridge
 *
 * WHAT: Frees all resources associated with bridge
 * WHY:  Prevent memory leaks
 * HOW:  Destroys generators, frees buffers, releases mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT void pr_pink_bridge_destroy(pr_pink_bridge_t bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Resets all generators and clears history
 * WHY:  Start fresh noise sequence for reproducibility
 * HOW:  Reseeds generators, clears buffers, resets statistics
 *
 * @param bridge Bridge to reset
 * @param new_seed New random seed (0 = time-based)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool pr_pink_bridge_reset(pr_pink_bridge_t bridge, uint32_t new_seed);

//=============================================================================
// Consolidation Timing Functions
//=============================================================================

/**
 * @brief Get next scheduled consolidation time
 *
 * WHAT: Returns timestamp of next consolidation event
 * WHY:  Schedule memory consolidation with 1/f timing
 * HOW:  Current time + fractal-modulated interval
 *
 * ALGORITHM:
 *   1. Get pink noise sample from consolidation generator
 *   2. Apply to base interval: interval = base * exp(amplitude * noise)
 *   3. Clamp to [min_interval, max_interval]
 *   4. Return current_time + interval
 *
 * @param bridge Pink noise bridge
 * @return Next consolidation time (milliseconds since epoch)
 *
 * COMPLEXITY: O(1)
 *
 * EXAMPLE:
 * @code
 * uint64_t next_time = pr_pink_next_consolidation_time(bridge);
 * if (current_time_ms >= next_time) {
 *     trigger_consolidation();
 *     next_time = pr_pink_next_consolidation_time(bridge);
 * }
 * @endcode
 */
NIMCP_EXPORT uint64_t pr_pink_next_consolidation_time(pr_pink_bridge_t bridge);

/**
 * @brief Check if consolidation event is due now
 *
 * WHAT: Returns true if current time >= scheduled consolidation time
 * WHY:  Convenience function for event loop integration
 *
 * @param bridge Pink noise bridge
 * @param current_time_ms Current time in milliseconds
 * @return true if consolidation should occur
 */
NIMCP_EXPORT bool pr_pink_should_consolidate_now(
    pr_pink_bridge_t bridge,
    uint64_t current_time_ms);

/**
 * @brief Mark consolidation as complete and schedule next
 *
 * WHAT: Records consolidation event and generates next time
 * WHY:  After consolidation, need to schedule next event
 *
 * @param bridge Pink noise bridge
 * @param current_time_ms Current time in milliseconds
 * @return Next scheduled consolidation time
 */
NIMCP_EXPORT uint64_t pr_pink_consolidation_complete(
    pr_pink_bridge_t bridge,
    uint64_t current_time_ms);

/**
 * @brief Set base consolidation interval
 *
 * WHAT: Changes the mean time between consolidation events
 * WHY:  Adjust consolidation frequency based on system state
 *
 * @param bridge Pink noise bridge
 * @param interval_ms Base interval in milliseconds
 * @return true on success, false if invalid interval
 */
NIMCP_EXPORT bool pr_pink_set_base_consolidation_interval(
    pr_pink_bridge_t bridge,
    float interval_ms);

/**
 * @brief Get current consolidation interval
 *
 * WHAT: Returns the current (modulated) consolidation interval
 * WHY:  Monitor consolidation timing behavior
 *
 * @param bridge Pink noise bridge
 * @return Current interval in milliseconds
 */
NIMCP_EXPORT float pr_pink_get_consolidation_interval(pr_pink_bridge_t bridge);

//=============================================================================
// Resonance Modulation Functions
//=============================================================================

/**
 * @brief Modulate resonance score with 1/f noise
 *
 * WHAT: Adds pink noise fluctuation to resonance score
 * WHY:  Prevent static memory hierarchies, enable serendipity
 * HOW:  resonance_out = base * (1 + amplitude * noise)
 *
 * @param bridge Pink noise bridge
 * @param base_score Base resonance score [0, 1]
 * @return Modulated resonance score [0, 1] (clamped)
 *
 * COMPLEXITY: O(1)
 *
 * EXAMPLE:
 * @code
 * float raw_resonance = compute_resonance(query, target);
 * float modulated = pr_pink_bridge_modulate_resonance(bridge, raw_resonance);
 * @endcode
 */
NIMCP_EXPORT float pr_pink_bridge_modulate_resonance(
    pr_pink_bridge_t bridge,
    float base_score);

/**
 * @brief Batch modulate resonance scores
 *
 * WHAT: Modulates array of scores with correlated noise
 * WHY:  Efficient batch processing for retrieval
 * HOW:  Uses continuous noise stream for temporal correlation
 *
 * @param bridge Pink noise bridge
 * @param scores Array of resonance scores (modified in place)
 * @param count Number of scores
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(count)
 */
NIMCP_EXPORT bool pr_pink_bridge_modulate_resonance_batch(
    pr_pink_bridge_t bridge,
    float* scores,
    size_t count);

/**
 * @brief Get current resonance noise value
 *
 * WHAT: Returns the last generated resonance noise sample
 * WHY:  Peek at current noise state without advancing generator
 *
 * @param bridge Pink noise bridge
 * @return Current noise value [-1, +1]
 */
NIMCP_EXPORT float pr_pink_get_resonance_noise(pr_pink_bridge_t bridge);

//=============================================================================
// Decay Modulation Functions
//=============================================================================

/**
 * @brief Modulate decay rate with 1/f noise
 *
 * WHAT: Adds pink noise fluctuation to memory decay rate
 * WHY:  Natural variability in how fast memories fade
 * HOW:  decay_out = base_rate * (1 + amplitude * noise)
 *
 * @param bridge Pink noise bridge
 * @param base_rate Base decay rate (per second)
 * @param tier Memory tier (may affect modulation)
 * @return Modulated decay rate (>= 0)
 *
 * COMPLEXITY: O(1)
 *
 * EXAMPLE:
 * @code
 * float base_decay = z_ladder_get_decay_rate(ladder, tier);
 * float modulated = pr_pink_modulate_decay(bridge, base_decay, tier);
 * @endcode
 */
NIMCP_EXPORT float pr_pink_modulate_decay(
    pr_pink_bridge_t bridge,
    float base_rate,
    pr_memory_tier_t tier);

/**
 * @brief Batch modulate decay for memory nodes
 *
 * WHAT: Modulates decay rates for array of nodes
 * WHY:  Efficient batch processing during consolidation
 *
 * @param bridge Pink noise bridge
 * @param nodes Array of memory nodes
 * @param count Number of nodes
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(count)
 *
 * Note: Modifies node->decay_rate in place
 */
NIMCP_EXPORT bool pr_pink_modulate_decay_batch(
    pr_pink_bridge_t bridge,
    pr_memory_node_t** nodes,
    size_t count);

/**
 * @brief Get current decay modulation factor
 *
 * WHAT: Returns multiplicative factor for decay modulation
 * WHY:  Apply same factor to multiple nodes
 *
 * @param bridge Pink noise bridge
 * @return Current decay factor (typically 0.9 to 1.1)
 */
NIMCP_EXPORT float pr_pink_get_decay_factor(pr_pink_bridge_t bridge);

//=============================================================================
// Quaternion Drift Functions
//=============================================================================

/**
 * @brief Apply 1/f drift to quaternion state
 *
 * WHAT: Adds small pink noise perturbations to quaternion
 * WHY:  Prevents overfitting, adds biological realism
 * HOW:  quat += amplitude * correlated_noise (clamped)
 *
 * @param bridge Pink noise bridge
 * @param quat Input quaternion (modified in place)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 *
 * Component ranges after drift:
 * - w (consolidation): [0, 1]
 * - x (emotion): [-1, +1]
 * - y (salience): [0, 1]
 * - z (accessibility): [0, 1]
 *
 * EXAMPLE:
 * @code
 * nimcp_quaternion_t state = pr_memory_node_get_state(node);
 * pr_pink_drift_quaternion(bridge, &state);
 * pr_memory_node_update_state(node, state);
 * @endcode
 */
NIMCP_EXPORT bool pr_pink_drift_quaternion(
    pr_pink_bridge_t bridge,
    nimcp_quaternion_t* quat);

/**
 * @brief Apply correlated 1/f drift to quaternion
 *
 * WHAT: Drift with explicit correlation matrix
 * WHY:  Use custom correlations for specific scenarios
 *
 * @param bridge Pink noise bridge
 * @param quat Input quaternion (modified in place)
 * @param correlation 4x4 correlation matrix (NULL for default)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool pr_pink_drift_quaternion_correlated(
    pr_pink_bridge_t bridge,
    nimcp_quaternion_t* quat,
    const float correlation[4][4]);

/**
 * @brief Get current quaternion noise vector
 *
 * WHAT: Returns the last generated 4D noise sample
 * WHY:  Inspect noise state or apply manually
 *
 * @param bridge Pink noise bridge
 * @param noise_out Output: 4-element array [w, x, y, z]
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool pr_pink_get_quat_noise(
    pr_pink_bridge_t bridge,
    float noise_out[4]);

/**
 * @brief Set quaternion correlation matrix
 *
 * WHAT: Updates correlation between quaternion components
 * WHY:  Tune cross-correlation for specific use cases
 *
 * @param bridge Pink noise bridge
 * @param correlation 4x4 correlation matrix (must be positive-definite)
 * @return true on success, false if matrix is invalid
 *
 * Note: Triggers Cholesky recomputation
 */
NIMCP_EXPORT bool pr_pink_set_quat_correlation(
    pr_pink_bridge_t bridge,
    const float correlation[4][4]);

//=============================================================================
// Entanglement Modulation Functions
//=============================================================================

/**
 * @brief Modulate entanglement edge weight
 *
 * WHAT: Adds 1/f fluctuation to edge weight
 * WHY:  Enable exploration of association space
 * HOW:  weight_out = weight * (1 + amplitude * noise)
 *
 * @param bridge Pink noise bridge
 * @param edge Edge to modulate (weight modified in place)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT bool pr_pink_modulate_edge_weight(
    pr_pink_bridge_t bridge,
    entangle_edge_t* edge);

/**
 * @brief Modulate all edges in entanglement graph
 *
 * WHAT: Applies 1/f fluctuation to all edge weights
 * WHY:  Efficient batch processing of entire graph
 *
 * @param bridge Pink noise bridge
 * @param graph Entanglement graph
 * @return Number of edges modulated
 *
 * COMPLEXITY: O(E) where E = edges
 *
 * Warning: This iterates all edges; use sparingly
 */
NIMCP_EXPORT size_t pr_pink_modulate_graph(
    pr_pink_bridge_t bridge,
    entangle_graph_t graph);

/**
 * @brief Get current edge weight noise
 *
 * WHAT: Returns last generated edge noise sample
 * WHY:  Apply same noise to related edges
 *
 * @param bridge Pink noise bridge
 * @return Current noise value [-1, +1]
 */
NIMCP_EXPORT float pr_pink_get_edge_noise(pr_pink_bridge_t bridge);

//=============================================================================
// Promotion Timing Functions
//=============================================================================

/**
 * @brief Get next promotion check time with fractal timing
 *
 * WHAT: Returns when to check for tier promotions
 * WHY:  Promotion checks should also have 1/f timing
 *
 * @param bridge Pink noise bridge
 * @return Next promotion check time (ms since epoch)
 */
NIMCP_EXPORT uint64_t pr_pink_next_promotion_check(pr_pink_bridge_t bridge);

/**
 * @brief Modulate promotion eligibility score
 *
 * WHAT: Adds noise to eligibility to prevent deterministic promotion
 * WHY:  Some variability in when memories get promoted
 *
 * @param bridge Pink noise bridge
 * @param base_eligibility Base eligibility [0, 1]
 * @return Modulated eligibility [0, 1]
 */
NIMCP_EXPORT float pr_pink_modulate_eligibility(
    pr_pink_bridge_t bridge,
    float base_eligibility);

//=============================================================================
// Retrieval Modulation Functions
//=============================================================================

/**
 * @brief Modulate retrieval latency with 1/f noise
 *
 * WHAT: Adds pink noise to retrieval response time
 * WHY:  Model reaction time variability
 *
 * @param bridge Pink noise bridge
 * @param base_latency_ms Base latency in milliseconds
 * @return Modulated latency (>= 0)
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT float pr_pink_modulate_retrieval_latency(
    pr_pink_bridge_t bridge,
    float base_latency_ms);

/**
 * @brief Perturb retrieval order based on noise
 *
 * WHAT: Adds small random perturbations to retrieval scores
 * WHY:  Prevent deterministic retrieval ordering
 *
 * @param bridge Pink noise bridge
 * @param scores Array of retrieval scores (modified in place)
 * @param count Number of scores
 * @return true on success, false on failure
 *
 * Note: Uses smaller amplitude than full resonance modulation
 */
NIMCP_EXPORT bool pr_pink_modulate_retrieval_order(
    pr_pink_bridge_t bridge,
    float* scores,
    size_t count);

//=============================================================================
// Fractal Analysis Functions
//=============================================================================

/**
 * @brief Analyze memory dynamics history for 1/f characteristics
 *
 * WHAT: Validates that generated noise exhibits 1/f spectrum
 * WHY:  Quality assurance for biological realism
 * HOW:  Uses DFA and spectral analysis from nimcp_fractal.h
 *
 * @param bridge Pink noise bridge
 * @param history History buffer to analyze
 * @param result Output: fractal analysis result
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(N log N) where N = history samples
 *
 * EXAMPLE:
 * @code
 * fractal_result_t result;
 * if (pr_pink_analyze_memory_dynamics(bridge, &history, &result) == 0) {
 *     printf("DFA exponent: %.3f (1.0 = pink)\n", result.dfa_exponent);
 * }
 * @endcode
 */
NIMCP_EXPORT int pr_pink_analyze_memory_dynamics(
    pr_pink_bridge_t bridge,
    const pr_pink_history_t* history,
    fractal_result_t* result);

/**
 * @brief Verify that target exhibits 1/f characteristics
 *
 * WHAT: Validates specific modulation target has pink spectrum
 * WHY:  Per-target quality check
 *
 * @param bridge Pink noise bridge
 * @param target Target to verify
 * @return true if target exhibits 1/f noise (alpha ~ 1.0)
 */
NIMCP_EXPORT bool pr_pink_verify_fractal(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target);

/**
 * @brief Get measured spectral exponent for target
 *
 * WHAT: Returns the alpha value from spectral analysis
 * WHY:  Quantify how "pink" the noise actually is
 *
 * @param bridge Pink noise bridge
 * @param target Target to query
 * @return Spectral exponent (1.0 = pink, 0 = white, 2 = brown)
 */
NIMCP_EXPORT float pr_pink_get_spectral_exponent(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target);

//=============================================================================
// Generator Control Functions
//=============================================================================

/**
 * @brief Set modulation amplitude for target
 *
 * WHAT: Changes the amplitude of noise modulation
 * WHY:  Runtime tuning of modulation strength
 *
 * @param bridge Pink noise bridge
 * @param target Target to modify
 * @param amplitude New amplitude [0, 1]
 * @return true on success, false if invalid
 */
NIMCP_EXPORT bool pr_pink_set_amplitude(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target,
    float amplitude);

/**
 * @brief Get modulation amplitude for target
 *
 * @param bridge Pink noise bridge
 * @param target Target to query
 * @return Current amplitude
 */
NIMCP_EXPORT float pr_pink_get_amplitude(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target);

/**
 * @brief Enable or disable modulation target
 *
 * WHAT: Turns pink noise on/off for specific target
 * WHY:  Selective enabling during experiments
 *
 * @param bridge Pink noise bridge
 * @param target Target to enable/disable
 * @param enabled true to enable, false to disable
 * @return true on success
 */
NIMCP_EXPORT bool pr_pink_set_enabled(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target,
    bool enabled);

/**
 * @brief Check if target is enabled
 *
 * @param bridge Pink noise bridge
 * @param target Target to query
 * @return true if enabled
 */
NIMCP_EXPORT bool pr_pink_is_enabled(
    pr_pink_bridge_t bridge,
    pr_pink_target_t target);

/**
 * @brief Advance all generators by time step
 *
 * WHAT: Steps all noise generators forward in time
 * WHY:  Synchronized time advancement for all targets
 *
 * @param bridge Pink noise bridge
 * @param dt_seconds Time step in seconds
 * @return true on success
 */
NIMCP_EXPORT bool pr_pink_step_all(pr_pink_bridge_t bridge, float dt_seconds);

/**
 * @brief Set global amplitude scale
 *
 * WHAT: Master volume control for all modulations
 * WHY:  Quick scaling of all noise effects
 *
 * @param bridge Pink noise bridge
 * @param amplitude Global amplitude [0, 1]
 * @return true on success
 */
NIMCP_EXPORT bool pr_pink_set_global_amplitude(
    pr_pink_bridge_t bridge,
    float amplitude);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Integrate bridge with theta-gamma oscillations
 *
 * WHAT: Couples pink noise to theta rhythm phase
 * WHY:  Coordinate noise with neural oscillation cycles
 * HOW:  Modulates noise amplitude based on theta phase
 *
 * At theta peak (encoding phase): noise amplitude enhanced
 * At theta trough (retrieval phase): noise amplitude reduced
 *
 * @param bridge Pink noise bridge
 * @param theta_phase Current theta phase [0, 2*pi]
 * @param theta_freq Theta frequency in Hz
 * @param coupling_strength Coupling strength [0, 1]
 * @return true on success
 */
NIMCP_EXPORT bool pr_pink_integrate_with_theta_gamma(
    pr_pink_bridge_t bridge,
    float theta_phase,
    float theta_freq,
    float coupling_strength);

/**
 * @brief Update theta phase for integrated operation
 *
 * @param bridge Pink noise bridge
 * @param dt_seconds Time step
 * @return New theta phase
 */
NIMCP_EXPORT float pr_pink_advance_theta_phase(
    pr_pink_bridge_t bridge,
    float dt_seconds);

/**
 * @brief Integrate bridge with Z-ladder
 *
 * WHAT: Couples pink noise parameters to Z-ladder tiers
 * WHY:  Different tiers may need different noise characteristics
 *
 * @param bridge Pink noise bridge
 * @param ladder Z-ladder to integrate with
 * @return true on success
 *
 * Effects:
 * - Z0: Higher resonance noise (working memory exploration)
 * - Z1: Moderate noise (consolidation variability)
 * - Z2: Lower noise (stable long-term storage)
 * - Z3: Minimal noise (permanent memories)
 */
NIMCP_EXPORT bool pr_pink_integrate_with_z_ladder(
    pr_pink_bridge_t bridge,
    z_ladder_t ladder);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Returns operational metrics
 * WHY:  Monitoring, debugging, optimization
 *
 * @param bridge Pink noise bridge
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool pr_pink_bridge_get_stats(
    pr_pink_bridge_t bridge,
    pr_pink_bridge_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Pink noise bridge
 */
NIMCP_EXPORT void pr_pink_bridge_reset_stats(pr_pink_bridge_t bridge);

/**
 * @brief Print diagnostic information to stdout
 *
 * WHAT: Human-readable summary of bridge state
 * WHY:  Debugging and monitoring
 *
 * @param bridge Pink noise bridge
 */
NIMCP_EXPORT void pr_pink_bridge_print_diagnostics(pr_pink_bridge_t bridge);

//=============================================================================
// History Management Functions
//=============================================================================

/**
 * @brief Create history buffer for analysis
 *
 * @param max_samples Maximum samples to store
 * @return History buffer or NULL on failure
 */
NIMCP_EXPORT pr_pink_history_t* pr_pink_history_create(size_t max_samples);

/**
 * @brief Destroy history buffer
 *
 * @param history Buffer to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_pink_history_destroy(pr_pink_history_t* history);

/**
 * @brief Get bridge's internal history buffer
 *
 * @param bridge Pink noise bridge
 * @return Internal history (do not modify directly)
 */
NIMCP_EXPORT const pr_pink_history_t* pr_pink_bridge_get_history(
    pr_pink_bridge_t bridge);

/**
 * @brief Clear history buffer
 *
 * @param bridge Pink noise bridge
 */
NIMCP_EXPORT void pr_pink_bridge_clear_history(pr_pink_bridge_t bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get target name as string
 *
 * @param target Modulation target
 * @return Human-readable name
 */
NIMCP_EXPORT const char* pr_pink_target_name(pr_pink_target_t target);

/**
 * @brief Get last error message
 *
 * WHAT: Returns description of last error
 * WHY:  Debugging failed operations
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* pr_pink_bridge_get_last_error(void);

/**
 * @brief Get current time in milliseconds
 *
 * Utility function for timestamp generation.
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_pink_bridge_current_time_ms(void);

/**
 * @brief Validate that bridge is functioning correctly
 *
 * WHAT: Self-test of bridge components
 * WHY:  Detect configuration or runtime issues
 *
 * @param bridge Pink noise bridge
 * @return true if all components valid
 */
NIMCP_EXPORT bool pr_pink_bridge_validate(pr_pink_bridge_t bridge);

//=============================================================================
// Error Codes
//=============================================================================

/** Success */
#define PR_PINK_BRIDGE_OK                    0

/** Null pointer argument */
#define PR_PINK_BRIDGE_ERROR_NULL           -1

/** Invalid configuration */
#define PR_PINK_BRIDGE_ERROR_CONFIG         -2

/** Memory allocation failed */
#define PR_PINK_BRIDGE_ERROR_ALLOC          -3

/** Invalid target */
#define PR_PINK_BRIDGE_ERROR_TARGET         -4

/** Generator error */
#define PR_PINK_BRIDGE_ERROR_GENERATOR      -5

/** Invalid parameter value */
#define PR_PINK_BRIDGE_ERROR_PARAM          -6

/** Target is disabled */
#define PR_PINK_BRIDGE_ERROR_DISABLED       -7

/** Correlation matrix not positive-definite */
#define PR_PINK_BRIDGE_ERROR_CORRELATION    -8

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_PINK_NOISE_BRIDGE_H */
