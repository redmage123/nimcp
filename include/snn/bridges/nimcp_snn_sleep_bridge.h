/**
 * @file nimcp_snn_sleep_bridge.h
 * @brief SNN-Sleep integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and sleep/consolidation system
 * WHY:  Enable spike-based sleep pattern detection and memory consolidation
 * HOW:  Detect sleep spindles, slow waves, REM via spike patterns
 *
 * BIOLOGICAL BASIS:
 * - Sleep spindles (12-15 Hz) facilitate memory consolidation
 * - Slow waves (<1 Hz) during NREM reflect synchronized down states
 * - REM sleep shows irregular, desynchronized spike patterns
 * - Spike replay during sleep consolidates episodic memories
 *
 * INTEGRATION:
 * - SNN → Sleep: Detect spindles, slow waves from population activity
 * - SNN → Sleep: Identify REM from high variability, low synchrony
 * - Sleep → SNN: Spindles enhance synaptic consolidation
 * - Sleep → SNN: Slow waves drive spike replay sequences
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_SLEEP_BRIDGE_H
#define NIMCP_SNN_SLEEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Sleep stage enumeration
 *
 * WHAT: Stages of sleep cycle
 * WHY:  Different spike patterns per stage
 * HOW:  Standard sleep staging (AASM)
 */
typedef enum {
    SNN_SLEEP_WAKE = 0,        /**< Awake, desynchronized */
    SNN_SLEEP_NREM1,           /**< Light sleep, theta */
    SNN_SLEEP_NREM2,           /**< Sleep spindles, K-complexes */
    SNN_SLEEP_NREM3,           /**< Slow wave sleep */
    SNN_SLEEP_REM,             /**< REM, rapid eye movement */
    SNN_SLEEP_UNKNOWN          /**< Indeterminate */
} snn_sleep_stage_t;

/**
 * @brief SNN-Sleep bridge configuration
 *
 * WHAT: Parameters for SNN-sleep integration
 * WHY:  Control sleep pattern detection and consolidation
 * HOW:  Thresholds for each sleep feature
 */
typedef struct snn_sleep_config_s {
    /* Spindle detection */
    float spindle_frequency;        /**< Center frequency (Hz, 12-15) */
    float spindle_bandwidth;        /**< Bandwidth (Hz, default: 3) */
    float spindle_min_duration_ms;  /**< Min duration (ms, default: 500) */
    float spindle_power_threshold;  /**< Power threshold for detection */

    /* Slow wave detection */
    float slow_wave_max_freq;       /**< Max frequency (Hz, default: 1) */
    float slow_wave_threshold;      /**< Amplitude threshold */
    float slow_wave_min_duration_ms; /**< Min duration (ms, default: 1000) */

    /* REM detection */
    float rem_density;              /**< Expected spike density in REM */
    float rem_variability_threshold; /**< CV threshold for REM */
    float rem_min_duration_ms;      /**< Min REM duration (ms) */

    /* Consolidation parameters */
    bool enable_replay;             /**< Enable spike replay */
    float replay_speed_factor;      /**< Replay speed (default: 10x) */
    float consolidation_strength;   /**< Synaptic strengthening factor */

    /* Population mapping */
    uint32_t cortical_population_id; /**< Cortical population to monitor */

    /* Update timing */
    float update_interval_ms;       /**< Detection update rate */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_sleep_config_t;

/**
 * @brief Sleep pattern state
 *
 * WHAT: Detected sleep patterns and metrics
 * WHY:  Track sleep architecture
 * HOW:  Counters and current state
 */
typedef struct snn_sleep_state_s {
    /* Current sleep stage */
    snn_sleep_stage_t sleep_stage;  /**< Current detected stage */
    float stage_duration_ms;        /**< Time in current stage */

    /* Spindle state */
    uint32_t spindle_count;         /**< Total spindles detected */
    float spindle_power;            /**< Current spindle power */
    bool spindle_active;            /**< Spindle currently detected */

    /* Slow wave state */
    uint32_t slow_wave_count;       /**< Total slow waves */
    float slow_wave_power;          /**< Current slow wave power */
    bool slow_wave_active;          /**< Slow wave currently detected */

    /* REM state */
    float rem_activity;             /**< REM activity index [0, 1] */
    float spike_variability;        /**< Coefficient of variation */

    /* Consolidation state */
    uint32_t replay_count;          /**< Number of replay events */
    float consolidation_progress;   /**< Consolidation progress [0, 1] */

    /* Statistics */
    uint32_t update_count;          /**< Total updates */
    float time_in_stage_ms[6];      /**< Time spent in each stage */
} snn_sleep_state_t;

/**
 * @brief SNN-Sleep bridge structure
 *
 * WHAT: Context for SNN-sleep integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and cached state
 */
typedef struct snn_sleep_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;             /**< SNN network */
    snn_sleep_config_t config;      /**< Bridge configuration */
    snn_sleep_state_t state;        /**< Current state */

    /* Populations */
    snn_population_t* cortical_pop; /**< Cortical population */

    /* Timing */
    float last_update_time;         /**< Last update timestamp (ms) */
    float total_time;               /**< Total monitoring time (ms) */

    /* Bio-async */
    bool bio_async_enabled;         /**< Bio-async connected */
    bio_module_context_t bio_ctx;   /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_sleep_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize sleep bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from sleep neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_sleep_config_default(snn_sleep_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-sleep bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable SNN-sleep integration
 * HOW:  Allocate context, set up connections
 *
 * @param config Bridge configuration
 * @param snn SNN network
 * @return Bridge instance or NULL on failure
 */
snn_sleep_bridge_t* snn_sleep_bridge_create(
    const snn_sleep_config_t* config,
    snn_network_t* snn
);

/**
 * @brief Destroy SNN-sleep bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_sleep_bridge_destroy(snn_sleep_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed sleep coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_sleep_bridge_connect_bio_async(snn_sleep_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_sleep_bridge_disconnect_bio_async(snn_sleep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_sleep_bridge_is_bio_async_connected(const snn_sleep_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Detect sleep patterns from spike activity
 * WHY:  Keep sleep staging current
 * HOW:  Analyze population activity, classify stage
 *
 * @param bridge Bridge to update
 * @param dt Time step in milliseconds
 * @return 0 on success, error code on failure
 */
int snn_sleep_bridge_update(snn_sleep_bridge_t* bridge, float dt);

//=============================================================================
// Sleep Pattern Detection
//=============================================================================

/**
 * @brief Detect sleep spindles
 *
 * WHAT: Identify spindle oscillations in spike patterns
 * WHY:  Spindles mark stage 2 sleep
 * HOW:  Band-pass filter population activity (12-15 Hz)
 *
 * @param bridge Bridge instance
 * @param population Population to analyze
 * @return true if spindle detected
 */
bool snn_sleep_detect_spindle(
    snn_sleep_bridge_t* bridge,
    snn_population_t* population
);

/**
 * @brief Detect slow waves
 *
 * WHAT: Identify slow oscillations (<1 Hz)
 * WHY:  Slow waves mark deep sleep (NREM3)
 * HOW:  Detect synchronized down states
 *
 * @param bridge Bridge instance
 * @param population Population to analyze
 * @return true if slow wave detected
 */
bool snn_sleep_detect_slow_wave(
    snn_sleep_bridge_t* bridge,
    snn_population_t* population
);

/**
 * @brief Detect REM sleep
 *
 * WHAT: Identify REM from spike patterns
 * WHY:  REM shows desynchronized, variable activity
 * HOW:  High spike variability, low synchrony
 *
 * @param bridge Bridge instance
 * @param population Population to analyze
 * @return true if REM detected
 */
bool snn_sleep_detect_rem(
    snn_sleep_bridge_t* bridge,
    snn_population_t* population
);

/**
 * @brief Classify sleep stage
 *
 * WHAT: Determine current sleep stage from patterns
 * WHY:  Integrate multiple features for staging
 * HOW:  Decision tree based on spindles, slow waves, REM
 *
 * @param bridge Bridge instance
 * @return Detected sleep stage
 */
snn_sleep_stage_t snn_sleep_classify_stage(snn_sleep_bridge_t* bridge);

//=============================================================================
// Memory Consolidation
//=============================================================================

/**
 * @brief Trigger memory consolidation
 *
 * WHAT: Enhance synaptic weights during sleep
 * WHY:  Sleep consolidates memories
 * HOW:  Strengthen recently active synapses
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int snn_sleep_consolidate_memory(snn_sleep_bridge_t* bridge);

/**
 * @brief Trigger spike replay
 *
 * WHAT: Replay recent spike sequences
 * WHY:  Replay consolidates episodic memories
 * HOW:  Replay at accelerated speed during sleep
 *
 * @param bridge Bridge instance
 * @param sequence_id Sequence to replay
 * @return 0 on success, error code on failure
 */
int snn_sleep_replay_sequence(
    snn_sleep_bridge_t* bridge,
    uint32_t sequence_id
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current sleep stage
 *
 * @param bridge Bridge to query
 * @return Current sleep stage
 */
snn_sleep_stage_t snn_sleep_get_stage(const snn_sleep_bridge_t* bridge);

/**
 * @brief Get sleep stage duration
 *
 * @param bridge Bridge to query
 * @return Duration in current stage (ms)
 */
float snn_sleep_get_stage_duration(const snn_sleep_bridge_t* bridge);

/**
 * @brief Get spindle count
 *
 * @param bridge Bridge to query
 * @return Total spindles detected
 */
uint32_t snn_sleep_get_spindle_count(const snn_sleep_bridge_t* bridge);

/**
 * @brief Get slow wave count
 *
 * @param bridge Bridge to query
 * @return Total slow waves detected
 */
uint32_t snn_sleep_get_slow_wave_count(const snn_sleep_bridge_t* bridge);

/**
 * @brief Get REM activity
 *
 * @param bridge Bridge to query
 * @return REM activity index [0, 1]
 */
float snn_sleep_get_rem_activity(const snn_sleep_bridge_t* bridge);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge to query
 * @param state Output state (copied)
 * @return 0 on success
 */
int snn_sleep_bridge_get_state(
    const snn_sleep_bridge_t* bridge,
    snn_sleep_state_t* state
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get sleep architecture statistics
 *
 * @param bridge Bridge to query
 * @param total_time Output: total monitoring time (ms)
 * @param time_in_stage Output: time per stage array[6] (ms)
 * @return 0 on success
 */
int snn_sleep_get_architecture(
    const snn_sleep_bridge_t* bridge,
    float* total_time,
    float* time_in_stage
);

/**
 * @brief Reset sleep statistics
 *
 * @param bridge Bridge to reset
 */
void snn_sleep_reset_stats(snn_sleep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_SLEEP_BRIDGE_H */
