/**
 * @file nimcp_snn_buffer_bridge.h
 * @brief SNN-Buffer bridge for spike train buffering and temporal processing
 *
 * WHAT: Bidirectional integration between SNN and circular buffer system
 * WHY:  Enable temporal spike pattern analysis via sliding window buffering
 * HOW:  Buffer spike trains in circular buffers for pattern matching and delays
 *
 * BIOLOGICAL BASIS:
 * - Neurons maintain temporal context via dendrite integration windows
 * - STDP requires spike history (pre/post timing)
 * - Working memory buffers recent spike activity
 * - Delay lines model axonal conduction delays
 *
 * INTEGRATION:
 * - Connects to snn_network_t for spike collection
 * - Connects to circular_buffer_t for temporal buffering
 * - Uses bio-async for spike event messaging
 * - Enables temporal pattern detection and replay
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_BUFFER_BRIDGE_H
#define NIMCP_SNN_BUFFER_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "middleware/buffering/nimcp_circular_buffer.h"
#include "async/nimcp_bio_async.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN buffer bridge configuration
 *
 * WHAT: Parameters for SNN-buffer integration
 * WHY:  Control buffering behavior and temporal windows
 * HOW:  Configure buffer sizes, window lengths, overflow handling
 */
typedef struct snn_buffer_config_s {
    /* Buffer parameters */
    size_t buffer_capacity;          /**< Max spike events per buffer */
    overflow_strategy_t overflow;    /**< What to do when buffer full */
    float temporal_window_ms;        /**< Temporal window for patterns (ms) */

    /* Buffering modes */
    bool enable_per_neuron_buffers;  /**< Separate buffer per neuron */
    bool enable_population_buffers;  /**< Separate buffer per population */
    bool enable_spike_replay;        /**< Support replay from buffer */
    bool enable_delay_lines;         /**< Implement axonal delays */

    /* Temporal processing */
    bool enable_pattern_detection;   /**< Detect repeating patterns */
    float min_pattern_isi_ms;        /**< Min ISI for pattern (ms) */
    float max_pattern_isi_ms;        /**< Max ISI for pattern (ms) */

    /* Delay configuration */
    float min_delay_ms;              /**< Minimum axonal delay */
    float max_delay_ms;              /**< Maximum axonal delay */
    float delay_std_ms;              /**< Delay variability */

    /* Integration flags */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float update_interval_ms;        /**< How often to update buffers */
} snn_buffer_config_t;

/**
 * @brief Buffering statistics
 *
 * WHAT: Metrics for spike buffering performance
 * WHY:  Monitor buffer utilization and detect issues
 * HOW:  Track buffer fills, overflows, patterns detected
 */
typedef struct snn_buffer_stats_s {
    uint64_t spikes_buffered;        /**< Total spikes buffered */
    uint64_t buffer_overflows;       /**< Buffer overflow events */
    uint64_t buffer_underflows;      /**< Read from empty */
    uint64_t patterns_detected;      /**< Repeating patterns found */
    uint64_t spikes_replayed;        /**< Spikes replayed */
    float avg_buffer_utilization;    /**< Average buffer fill % */
    float peak_buffer_utilization;   /**< Peak buffer fill % */
} snn_buffer_stats_t;

/**
 * @brief SNN-Buffer bridge structure
 *
 * WHAT: Context for SNN-buffer integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and buffering state
 */
typedef struct snn_buffer_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;          /**< SNN network being buffered */
    circular_buffer_t** buffers;     /**< Array of buffers (per neuron or pop) */
    uint32_t n_buffers;              /**< Number of buffers */
    snn_buffer_config_t config;      /**< Bridge configuration */
    snn_buffer_stats_t stats;        /**< Buffering statistics */

    /* Buffering state */
    bool connected;                  /**< Bridge active */
    float last_update_time;          /**< Last update timestamp (ms) */
    uint32_t* buffer_map;            /**< Neuron/population ID to buffer index */

    /* Delay line state */
    uint64_t* delayed_spike_times;   /**< Delayed spike delivery times */
    uint32_t* delayed_spike_neurons; /**< Neuron IDs for delayed spikes */
    uint32_t n_delayed_spikes;       /**< Number of pending delayed spikes */

    /* Pattern detection */
    uint32_t* pattern_hashes;        /**< Hash of detected patterns */
    uint32_t n_patterns;             /**< Number of unique patterns */

    /* Bio-async */
    bool bio_async_enabled;          /**< Bio-async connected */
    bio_module_context_t bio_ctx;    /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_buffer_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize buffer config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from temporal integration literature
 *
 * @param config Config to initialize
 */
void snn_buffer_config_default(snn_buffer_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-buffer bridge
 *
 * WHAT: Initialize bidirectional bridge between SNN and buffers
 * WHY:  Enable temporal spike buffering
 * HOW:  Allocate context, create buffers, set up connections
 *
 * @param config Bridge configuration
 * @param network SNN network to buffer
 * @return Bridge instance or NULL on failure
 */
snn_buffer_bridge_t* snn_buffer_bridge_create(
    const snn_buffer_config_t* config,
    snn_network_t* network
);

/**
 * @brief Destroy SNN-buffer bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free buffers
 *
 * @param bridge Bridge to destroy
 */
void snn_buffer_bridge_destroy(snn_buffer_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async spike messaging
 * WHY:  Distributed spike coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_connect_bio_async(snn_buffer_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_buffer_bridge_disconnect_bio_async(snn_buffer_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_buffer_bridge_is_bio_async_connected(const snn_buffer_bridge_t* bridge);

//=============================================================================
// Buffering Functions
//=============================================================================

/**
 * @brief Process spike buffering
 *
 * WHAT: Buffer incoming spikes for temporal processing
 * WHY:  Enable sliding window analysis and pattern detection
 * HOW:  Push spikes to appropriate circular buffers
 *
 * ALGORITHM:
 * 1. For each spike:
 *    a. Determine target buffer (neuron or population)
 *    b. Push to circular buffer
 *    c. Handle overflow per strategy
 *    d. If delay enabled, schedule delayed delivery
 * 2. Update statistics
 *
 * @param bridge Bridge for buffering
 * @param spikes_in Input spike array [n_spikes]
 * @param n_spikes Number of input spikes
 * @param spikes_out Output spike array (delayed/buffered) [n_out capacity]
 * @param n_out_capacity Maximum output spikes
 * @param n_out_actual Actual number of output spikes
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_process(
    snn_buffer_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
);

/**
 * @brief Update buffering state
 *
 * WHAT: Process delayed spikes, detect patterns
 * WHY:  Advance temporal processing
 * HOW:  Check delayed spike delivery times, analyze patterns
 *
 * @param bridge Bridge to update
 * @param dt Timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_update(snn_buffer_bridge_t* bridge, float dt);

/**
 * @brief Get buffered spikes in temporal window
 *
 * WHAT: Retrieve spikes from buffer within time window
 * WHY:  Access recent spike history for pattern analysis
 * HOW:  Read from circular buffer in temporal range
 *
 * @param bridge Bridge to query
 * @param buffer_id Buffer index (neuron or population)
 * @param window_ms Temporal window (ms back from now)
 * @param spikes_out Output spike array [capacity]
 * @param capacity Maximum spikes to retrieve
 * @param n_spikes_actual Actual number retrieved
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_get_window(
    const snn_buffer_bridge_t* bridge,
    uint32_t buffer_id,
    float window_ms,
    snn_spike_t* spikes_out,
    uint32_t capacity,
    uint32_t* n_spikes_actual
);

/**
 * @brief Clear buffer contents
 *
 * WHAT: Reset buffer to empty state
 * WHY:  Discard history on reset or task change
 * HOW:  Clear circular buffer
 *
 * @param bridge Bridge with buffer to clear
 * @param buffer_id Buffer index
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_clear_buffer(
    snn_buffer_bridge_t* bridge,
    uint32_t buffer_id
);

/**
 * @brief Clear all buffers
 *
 * @param bridge Bridge to clear
 */
void snn_buffer_bridge_clear_all(snn_buffer_bridge_t* bridge);

//=============================================================================
// Delay Lines
//=============================================================================

/**
 * @brief Add delayed spike
 *
 * WHAT: Schedule spike for delayed delivery
 * WHY:  Model axonal conduction delays
 * HOW:  Store spike with delivery time
 *
 * BIOLOGICAL BASIS:
 * - Axons have finite conduction velocity (1-100 m/s)
 * - Myelination affects delay (0.1-10 ms typical)
 * - Synaptic transmission adds 0.5-1 ms
 *
 * @param bridge Bridge for delay
 * @param spike Spike to delay
 * @param delay_ms Delay duration (ms)
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_add_delayed_spike(
    snn_buffer_bridge_t* bridge,
    const snn_spike_t* spike,
    float delay_ms
);

/**
 * @brief Process delayed spike delivery
 *
 * WHAT: Deliver spikes whose delay has elapsed
 * WHY:  Implement temporal spike propagation
 * HOW:  Check delivery times, emit ready spikes
 *
 * @param bridge Bridge with delayed spikes
 * @param current_time_us Current simulation time (µs)
 * @param spikes_out Output array for ready spikes
 * @param capacity Max spikes to deliver
 * @param n_delivered Actual spikes delivered
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_deliver_delayed_spikes(
    snn_buffer_bridge_t* bridge,
    uint64_t current_time_us,
    snn_spike_t* spikes_out,
    uint32_t capacity,
    uint32_t* n_delivered
);

//=============================================================================
// Pattern Detection
//=============================================================================

/**
 * @brief Detect repeating spike patterns
 *
 * WHAT: Find repeating temporal spike sequences
 * WHY:  Identify learned patterns and oscillations
 * HOW:  Hash spike timing sequences, find repeats
 *
 * @param bridge Bridge with buffered spikes
 * @param buffer_id Buffer to analyze
 * @param min_pattern_length Minimum pattern length (spikes)
 * @param pattern_detected Output: true if pattern found
 * @return 0 on success, error code on failure
 */
int snn_buffer_bridge_detect_pattern(
    snn_buffer_bridge_t* bridge,
    uint32_t buffer_id,
    uint32_t min_pattern_length,
    bool* pattern_detected
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get buffering statistics
 *
 * @param bridge Bridge to query
 * @param stats Output: statistics (copied)
 * @return 0 on success
 */
int snn_buffer_bridge_get_stats(
    const snn_buffer_bridge_t* bridge,
    snn_buffer_stats_t* stats
);

/**
 * @brief Reset buffering statistics
 *
 * @param bridge Bridge to reset
 */
void snn_buffer_bridge_reset_stats(snn_buffer_bridge_t* bridge);

/**
 * @brief Get buffer utilization
 *
 * WHAT: Calculate buffer fill percentage
 * WHY:  Monitor buffer health
 * HOW:  Query circular buffer size/capacity
 *
 * @param bridge Bridge to query
 * @param buffer_id Buffer index
 * @return Utilization [0, 100]
 */
float snn_buffer_bridge_get_utilization(
    const snn_buffer_bridge_t* bridge,
    uint32_t buffer_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_BUFFER_BRIDGE_H */
