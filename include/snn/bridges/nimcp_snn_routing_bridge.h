/**
 * @file nimcp_snn_routing_bridge.h
 * @brief SNN-Routing bridge for spike packet routing between SNN layers
 *
 * WHAT: Bidirectional integration between SNN and spike routing system
 * WHY:  Enable priority-based spike routing across SNN populations and modules
 * HOW:  Route spike packets using thalamic router with attention-based gating
 *
 * BIOLOGICAL BASIS:
 * - Thalamus routes sensory/cognitive information to cortical areas
 * - Priority-based routing (high-priority spikes bypass queue)
 * - Attention gates determine which spike trains propagate
 * - Burst mode for salient information, tonic for background
 *
 * INTEGRATION:
 * - Connects to snn_network_t for spike generation
 * - Connects to thalamic_router_t for routing infrastructure
 * - Uses bio-async for distributed spike messaging
 * - Routes between SNN populations and external modules
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_ROUTING_BRIDGE_H
#define NIMCP_SNN_ROUTING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "async/nimcp_bio_async.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN routing bridge configuration
 *
 * WHAT: Parameters for SNN-routing integration
 * WHY:  Control spike routing behavior and priority
 * HOW:  Configure attention gating, priority levels, buffering
 */
typedef struct snn_routing_config_s {
    /* Routing parameters */
    float attention_threshold;       /**< Min attention for spike propagation [0, 1] */
    signal_priority_t default_priority; /**< Default spike packet priority */
    uint32_t max_queue_size;         /**< Max queued spike packets */
    uint32_t max_destinations;       /**< Max fan-out per spike source */

    /* Spike routing modes */
    bool enable_burst_routing;       /**< Route burst spikes at high priority */
    bool enable_population_broadcast; /**< Broadcast population spikes */
    bool enable_selective_routing;   /**< Route only high-rate neurons */
    float burst_detection_threshold; /**< ISI threshold for burst detection (ms) */

    /* Attention-based filtering */
    bool enable_attention_gating;    /**< Apply attention weights to routing */
    float min_firing_rate_hz;        /**< Min rate for routing consideration */
    float max_firing_rate_hz;        /**< Max rate (clamp) */

    /* Integration flags */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float update_interval_ms;        /**< How often to update routing */
} snn_routing_config_t;

/**
 * @brief Spike routing statistics
 *
 * WHAT: Metrics for spike routing performance
 * WHY:  Monitor routing efficiency and detect issues
 * HOW:  Track routed/dropped spikes, latency
 */
typedef struct snn_routing_stats_s {
    uint64_t spikes_routed;          /**< Total spikes routed */
    uint64_t spikes_dropped;         /**< Spikes dropped (queue full) */
    uint64_t spikes_filtered;        /**< Spikes filtered by attention */
    uint64_t bursts_detected;        /**< Burst spike sequences detected */
    float avg_routing_latency_ms;    /**< Average routing latency */
    float throughput_hz;             /**< Spikes per second */
    uint32_t active_routes;          /**< Number of active routes */
} snn_routing_stats_t;

/**
 * @brief SNN-Routing bridge structure
 *
 * WHAT: Context for SNN-routing integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and routing state
 */
typedef struct snn_routing_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;          /**< SNN network being routed */
    thalamic_router_t* router;       /**< Thalamic router */
    snn_routing_config_t config;     /**< Bridge configuration */
    snn_routing_stats_t stats;       /**< Routing statistics */

    /* Routing state */
    bool connected;                  /**< Bridge active */
    float last_update_time;          /**< Last update timestamp (ms) */
    uint32_t* route_map;             /**< Population ID to dest ID mapping */
    float* attention_weights;        /**< Per-population attention weights */

    /* Burst detection */
    uint64_t* last_spike_time_us;    /**< Last spike time per neuron */
    bool* in_burst_mode;             /**< Burst mode flag per neuron */

    /* Bio-async */
    bool bio_async_enabled;          /**< Bio-async connected */
    bio_module_context_t bio_ctx;    /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_routing_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize routing config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from thalamic routing literature
 *
 * @param config Config to initialize
 */
void snn_routing_config_default(snn_routing_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-routing bridge
 *
 * WHAT: Initialize bidirectional bridge between SNN and router
 * WHY:  Enable spike routing across populations
 * HOW:  Allocate context, set up connections, register callbacks
 *
 * @param config Bridge configuration
 * @param network SNN network to route
 * @param router Thalamic router for routing
 * @return Bridge instance or NULL on failure
 */
snn_routing_bridge_t* snn_routing_bridge_create(
    const snn_routing_config_t* config,
    snn_network_t* network,
    thalamic_router_t* router
);

/**
 * @brief Destroy SNN-routing bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_routing_bridge_destroy(snn_routing_bridge_t* bridge);

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
int snn_routing_bridge_connect_bio_async(snn_routing_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_routing_bridge_disconnect_bio_async(snn_routing_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_routing_bridge_is_bio_async_connected(const snn_routing_bridge_t* bridge);

//=============================================================================
// Routing Functions
//=============================================================================

/**
 * @brief Process spike routing for SNN populations
 *
 * WHAT: Route spikes from SNN to destinations via thalamic router
 * WHY:  Distribute spike information across modules
 * HOW:  Convert spike trains to routed signals, apply attention
 *
 * ALGORITHM:
 * 1. For each population with spikes:
 *    a. Detect bursts (ISI < threshold)
 *    b. Create routed signal packets
 *    c. Set priority (high for bursts, normal otherwise)
 *    d. Apply attention gating
 *    e. Route via thalamic router
 * 2. Update statistics
 *
 * @param bridge Bridge for routing
 * @param spikes_in Input spike array [n_spikes]
 * @param n_spikes Number of input spikes
 * @param spikes_out Output spike array (routed) [n_out capacity]
 * @param n_out_capacity Maximum output spikes
 * @param n_out_actual Actual number of output spikes
 * @return 0 on success, error code on failure
 */
int snn_routing_bridge_process(
    snn_routing_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
);

/**
 * @brief Update routing state
 *
 * WHAT: Update attention weights and routing tables
 * WHY:  Adapt routing to network state
 * HOW:  Query router attention, update burst detection
 *
 * @param bridge Bridge to update
 * @param dt Timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_routing_bridge_update(snn_routing_bridge_t* bridge, float dt);

/**
 * @brief Set attention weight for population
 *
 * WHAT: Modulate routing strength for population
 * WHY:  Top-down attention control
 * HOW:  Store weight, apply to future routing
 *
 * @param bridge Bridge to configure
 * @param pop_id Population ID
 * @param attention Attention weight [0, 1]
 * @return 0 on success, error code on failure
 */
int snn_routing_bridge_set_attention(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    float attention
);

/**
 * @brief Get attention weight for population
 *
 * @param bridge Bridge to query
 * @param pop_id Population ID
 * @param attention Output: attention weight
 * @return 0 on success, error code on failure
 */
int snn_routing_bridge_get_attention(
    const snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    float* attention
);

/**
 * @brief Add routing destination for population
 *
 * WHAT: Configure where population spikes are routed
 * WHY:  Define routing topology
 * HOW:  Add to route map
 *
 * @param bridge Bridge to configure
 * @param pop_id Source population
 * @param dest_id Destination module ID
 * @return 0 on success, error code on failure
 */
int snn_routing_bridge_add_route(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t dest_id
);

/**
 * @brief Remove routing destination
 *
 * @param bridge Bridge to configure
 * @param pop_id Source population
 * @param dest_id Destination to remove
 * @return 0 on success, error code on failure
 */
int snn_routing_bridge_remove_route(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t dest_id
);

/**
 * @brief Clear all routes
 *
 * WHAT: Remove all routing entries
 * WHY:  Reset routing state
 * HOW:  Clear route map
 *
 * @param bridge Bridge to clear
 */
void snn_routing_bridge_clear_routes(snn_routing_bridge_t* bridge);

//=============================================================================
// Burst Detection
//=============================================================================

/**
 * @brief Detect burst spikes in population
 *
 * WHAT: Identify burst spike patterns (short ISI)
 * WHY:  Bursts encode salient information, should be high priority
 * HOW:  Check ISI < burst_threshold for consecutive spikes
 *
 * BIOLOGICAL BASIS:
 * - Thalamic neurons switch between burst and tonic modes
 * - Bursts indicate wake-up from hyperpolarization (salient events)
 * - Tonic mode for steady-state information transmission
 *
 * @param bridge Bridge with burst detection state
 * @param pop_id Population to check
 * @param neuron_idx Neuron index within population
 * @param spike_time_us Current spike time
 * @return true if burst detected
 */
bool snn_routing_bridge_detect_burst(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t neuron_idx,
    uint64_t spike_time_us
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get routing statistics
 *
 * @param bridge Bridge to query
 * @param stats Output: statistics (copied)
 * @return 0 on success
 */
int snn_routing_bridge_get_stats(
    const snn_routing_bridge_t* bridge,
    snn_routing_stats_t* stats
);

/**
 * @brief Reset routing statistics
 *
 * @param bridge Bridge to reset
 */
void snn_routing_bridge_reset_stats(snn_routing_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_ROUTING_BRIDGE_H */
