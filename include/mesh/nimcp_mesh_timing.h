/**
 * @file nimcp_mesh_timing.h
 * @brief Hierarchical Pink Noise Timing for Mesh Network
 *
 * WHAT: Bio-plausible timing with pink noise jitter at each hierarchy level
 * WHY:  Prevent synchronized failures, enable biological realism
 * HOW:  Different timing characteristics at System, Hemisphere, Layer, Ordering levels
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    HIERARCHICAL TIMING SYSTEM                           │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  SYSTEM LEVEL         ┌─────────────────┐  base: 100ms, jitter: 50ms   │
 * │  (Global coord)       │   Pink Noise    │  Slow, high variability      │
 * │                       │   Generator     │                               │
 * │                       └────────┬────────┘                               │
 * │                                │                                        │
 * │  HEMISPHERE LEVEL     ┌───────┴────────┐  base: 50ms, jitter: 25ms     │
 * │  (Channel coord)      │   Pink Noise   │  Medium timescale             │
 * │                       │   Generator    │                                │
 * │                       └───────┬────────┘                               │
 * │                               │                                        │
 * │  LAYER LEVEL          ┌──────┴──────┐    base: 10ms, jitter: 5ms       │
 * │  (Local coord)        │ Pink Noise  │    Fast, low jitter              │
 * │                       │ Generator   │                                   │
 * │                       └──────┬──────┘                                  │
 * │                              │                                         │
 * │  ORDERING LEVEL       ┌─────┴─────┐      base: 5ms, jitter: 1ms        │
 * │  (Raft consensus)     │Pink Noise │      Very fast, minimal jitter     │
 * │                       │Generator  │                                    │
 * │                       └───────────┘                                    │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL MOTIVATION:
 * - Neural timing exhibits 1/f noise spectrum (Milstein et al., 2009)
 * - Hierarchical timescales match cortical processing
 * - Pink noise prevents pathological synchronization
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_TIMING_H
#define NIMCP_MESH_TIMING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mesh/nimcp_mesh_types.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Number of hierarchy levels */
#define MESH_TIMING_NUM_LEVELS              4

/** @brief System level timing defaults */
#define MESH_TIMING_SYSTEM_BASE_MS          100.0f
#define MESH_TIMING_SYSTEM_JITTER_MS        50.0f
#define MESH_TIMING_SYSTEM_MIN_MS           50.0f
#define MESH_TIMING_SYSTEM_MAX_MS           200.0f

/** @brief Hemisphere level timing defaults */
#define MESH_TIMING_HEMISPHERE_BASE_MS      50.0f
#define MESH_TIMING_HEMISPHERE_JITTER_MS    25.0f
#define MESH_TIMING_HEMISPHERE_MIN_MS       25.0f
#define MESH_TIMING_HEMISPHERE_MAX_MS       100.0f

/** @brief Layer level timing defaults */
#define MESH_TIMING_LAYER_BASE_MS           10.0f
#define MESH_TIMING_LAYER_JITTER_MS         5.0f
#define MESH_TIMING_LAYER_MIN_MS            5.0f
#define MESH_TIMING_LAYER_MAX_MS            20.0f

/** @brief Ordering level timing defaults */
#define MESH_TIMING_ORDERING_BASE_MS        5.0f
#define MESH_TIMING_ORDERING_JITTER_MS      1.0f
#define MESH_TIMING_ORDERING_MIN_MS         3.0f
#define MESH_TIMING_ORDERING_MAX_MS         10.0f

/** @brief Pink noise spectral exponent (1/f^alpha) */
#define MESH_TIMING_PINK_ALPHA              1.0f

/** @brief Default random seed (0 = time-based) */
#define MESH_TIMING_DEFAULT_SEED            0

/* ============================================================================
 * Timing Level Enumeration
 * ============================================================================ */

/**
 * @brief Hierarchy level for timing
 *
 * WHAT: Different timing levels in the coordinator hierarchy
 * WHY:  Each level has different timing requirements
 */
typedef enum mesh_timing_level {
    MESH_TIMING_LEVEL_SYSTEM = 0,       /**< System-wide coordination (slowest) */
    MESH_TIMING_LEVEL_HEMISPHERE,       /**< Hemisphere/channel coordination */
    MESH_TIMING_LEVEL_LAYER,            /**< Layer-level coordination */
    MESH_TIMING_LEVEL_ORDERING          /**< Ordering service (fastest) */
} mesh_timing_level_t;

/* ============================================================================
 * Timing Configuration
 * ============================================================================ */

/**
 * @brief Configuration for a single timing level
 *
 * WHAT: Timing parameters for one hierarchy level
 * WHY:  Configure base interval and jitter characteristics
 */
typedef struct mesh_timing_level_config {
    float base_interval_ms;             /**< Base timing interval */
    float jitter_amplitude_ms;          /**< Pink noise amplitude */
    float min_interval_ms;              /**< Minimum allowed interval */
    float max_interval_ms;              /**< Maximum allowed interval */
    float pink_alpha;                   /**< Spectral exponent (typically 1.0) */
} mesh_timing_level_config_t;

/**
 * @brief Hierarchical timing configuration
 *
 * WHAT: Configuration for all timing levels
 * WHY:  Configure entire timing hierarchy at once
 */
typedef struct mesh_hierarchical_timing_config {
    mesh_timing_level_config_t levels[MESH_TIMING_NUM_LEVELS];
    uint32_t random_seed;               /**< Random seed (0 = time-based) */
    bool enable_adaptation;             /**< Adaptive timing based on load */
    float adaptation_rate;              /**< How fast to adapt (0-1) */
} mesh_hierarchical_timing_config_t;

/* ============================================================================
 * Timing Statistics
 * ============================================================================ */

/**
 * @brief Statistics for a timing level
 */
typedef struct mesh_timing_level_stats {
    uint64_t samples_generated;         /**< Total intervals generated */
    float mean_interval_ms;             /**< Mean generated interval */
    float std_interval_ms;              /**< Standard deviation */
    float min_generated_ms;             /**< Minimum generated */
    float max_generated_ms;             /**< Maximum generated */
    float measured_alpha;               /**< Measured spectral exponent */
} mesh_timing_level_stats_t;

/**
 * @brief Statistics for entire timing hierarchy
 */
typedef struct mesh_hierarchical_timing_stats {
    mesh_timing_level_stats_t levels[MESH_TIMING_NUM_LEVELS];
    uint64_t total_samples;             /**< Total across all levels */
    float overall_jitter_factor;        /**< Overall jitter measure */
} mesh_hierarchical_timing_stats_t;

/* ============================================================================
 * Opaque Context Types
 * ============================================================================ */

/**
 * @brief Opaque hierarchical timing context
 */
typedef struct mesh_hierarchical_timing_internal* mesh_hierarchical_timing_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create default hierarchical timing configuration
 *
 * WHAT: Returns biologically-motivated defaults
 * WHY:  Easy starting point for timing configuration
 *
 * @return Default configuration
 */
mesh_hierarchical_timing_config_t mesh_timing_default_config(void);

/**
 * @brief Create default timing configuration for single level
 *
 * @param level The timing level
 * @return Default configuration for that level
 */
mesh_timing_level_config_t mesh_timing_default_level_config(mesh_timing_level_t level);

/**
 * @brief Create hierarchical timing context
 *
 * WHAT: Initialize timing generators for all hierarchy levels
 * WHY:  Generate pink noise timing intervals
 * HOW:  Create one pink noise generator per level
 *
 * @param config Configuration (NULL for defaults)
 * @return Timing context or NULL on failure
 */
mesh_hierarchical_timing_t mesh_timing_create(
    const mesh_hierarchical_timing_config_t* config
);

/**
 * @brief Destroy hierarchical timing context
 *
 * WHAT: Free all timing resources
 * WHY:  Prevent memory leaks
 *
 * @param timing Context to destroy
 */
void mesh_timing_destroy(mesh_hierarchical_timing_t timing);

/* ============================================================================
 * Interval Generation
 * ============================================================================ */

/**
 * @brief Generate next timing interval for a level
 *
 * WHAT: Get next pink-noise-jittered interval
 * WHY:  Bio-plausible timing prevents synchronization
 * HOW:  base + (pink_noise * jitter_amplitude), clamped to [min, max]
 *
 * @param timing Timing context
 * @param level Hierarchy level
 * @return Interval in milliseconds
 */
float mesh_timing_next_interval(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
);

/**
 * @brief Generate next interval in nanoseconds
 *
 * WHAT: Convenience function returning nanoseconds
 * WHY:  Match timestamp resolution
 *
 * @param timing Timing context
 * @param level Hierarchy level
 * @return Interval in nanoseconds
 */
uint64_t mesh_timing_next_interval_ns(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
);

/**
 * @brief Generate batch of timing intervals
 *
 * WHAT: Generate multiple intervals at once
 * WHY:  Efficient for precomputing timing schedules
 *
 * @param timing Timing context
 * @param level Hierarchy level
 * @param intervals Output array (must be allocated)
 * @param count Number of intervals to generate
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_generate_batch(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    float* intervals,
    size_t count
);

/* ============================================================================
 * Heartbeat and Timeout Functions
 * ============================================================================ */

/**
 * @brief Get heartbeat interval for coordinator level
 *
 * WHAT: Appropriate heartbeat interval for coordinator hierarchy
 * WHY:  Leaders send heartbeats to followers at level-appropriate rate
 *
 * @param timing Timing context
 * @param level Coordinator level
 * @return Heartbeat interval in milliseconds
 */
float mesh_timing_heartbeat_interval(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
);

/**
 * @brief Get election timeout for coordinator level
 *
 * WHAT: Randomized election timeout
 * WHY:  Prevent split votes in leader election
 * HOW:  2-3x heartbeat interval with pink noise jitter
 *
 * @param timing Timing context
 * @param level Coordinator level
 * @return Election timeout in milliseconds
 */
float mesh_timing_election_timeout(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
);

/**
 * @brief Get transaction timeout for a level
 *
 * WHAT: Appropriate timeout for transaction processing
 * WHY:  Different levels process at different speeds
 *
 * @param timing Timing context
 * @param level Processing level
 * @param base_timeout Base timeout to add jitter to
 * @return Timeout in milliseconds
 */
float mesh_timing_transaction_timeout(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    float base_timeout
);

/* ============================================================================
 * Adaptive Timing
 * ============================================================================ */

/**
 * @brief Report observed latency for adaptation
 *
 * WHAT: Feed back observed timing for adaptive adjustment
 * WHY:  Adapt timing to actual network conditions
 * HOW:  Exponential moving average update
 *
 * @param timing Timing context
 * @param level Hierarchy level
 * @param observed_ms Observed latency in milliseconds
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_report_latency(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    float observed_ms
);

/**
 * @brief Get current adaptation factor for a level
 *
 * WHAT: How much timing has adapted from base
 * WHY:  Monitor adaptation behavior
 *
 * @param timing Timing context
 * @param level Hierarchy level
 * @return Adaptation factor (1.0 = no adaptation)
 */
float mesh_timing_get_adaptation_factor(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
);

/**
 * @brief Reset adaptation to baseline
 *
 * WHAT: Clear adaptive adjustments
 * WHY:  Start fresh after configuration change
 *
 * @param timing Timing context
 * @param level Level to reset (or all if level is NUM_LEVELS)
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_reset_adaptation(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
);

/* ============================================================================
 * Configuration Update
 * ============================================================================ */

/**
 * @brief Update timing configuration for a level
 *
 * WHAT: Modify timing parameters at runtime
 * WHY:  Tune timing based on operational experience
 *
 * @param timing Timing context
 * @param level Level to update
 * @param config New configuration
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_update_level_config(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    const mesh_timing_level_config_t* config
);

/**
 * @brief Reseed random number generators
 *
 * WHAT: Reset RNG state with new seed
 * WHY:  Reproducibility or fresh randomness
 *
 * @param timing Timing context
 * @param seed New seed (0 = time-based)
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_reseed(
    mesh_hierarchical_timing_t timing,
    uint32_t seed
);

/* ============================================================================
 * Statistics and Debugging
 * ============================================================================ */

/**
 * @brief Get timing statistics
 *
 * @param timing Timing context
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_get_stats(
    mesh_hierarchical_timing_t timing,
    mesh_hierarchical_timing_stats_t* stats
);

/**
 * @brief Get statistics for single level
 *
 * @param timing Timing context
 * @param level Target level
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_get_level_stats(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    mesh_timing_level_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param timing Timing context
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_timing_reset_stats(mesh_hierarchical_timing_t timing);

/**
 * @brief Validate pink noise quality
 *
 * WHAT: Test if generated intervals match expected 1/f spectrum
 * WHY:  Quality assurance for biological realism
 *
 * @param timing Timing context
 * @param level Level to validate
 * @param num_samples Number of samples to test
 * @return true if valid pink noise, false otherwise
 */
bool mesh_timing_validate_pink_noise(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    uint32_t num_samples
);

/**
 * @brief Print timing debug information
 *
 * @param timing Timing context
 */
void mesh_timing_print_debug(mesh_hierarchical_timing_t timing);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get level name as string
 *
 * @param level Timing level
 * @return Level name
 */
const char* mesh_timing_level_to_string(mesh_timing_level_t level);

/**
 * @brief Convert coordinator level to timing level
 *
 * WHAT: Map coordinator hierarchy to timing hierarchy
 * WHY:  Coordinators use timing at their level
 *
 * @param coord_level Coordinator level (from nimcp_mesh_coordinator.h)
 * @return Corresponding timing level
 */
mesh_timing_level_t mesh_timing_level_from_coord_level(uint32_t coord_level);

/**
 * @brief Calculate expected convergence time
 *
 * WHAT: Estimate time for gossip convergence at a level
 * WHY:  Plan for consensus timing
 * HOW:  O(log N) rounds at level timing rate
 *
 * @param timing Timing context
 * @param level Hierarchy level
 * @param num_participants Number of participants
 * @return Expected convergence time in milliseconds
 */
float mesh_timing_expected_convergence(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    uint32_t num_participants
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_TIMING_H */
