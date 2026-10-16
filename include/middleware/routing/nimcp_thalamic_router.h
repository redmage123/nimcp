//=============================================================================
// nimcp_thalamic_router.h - Attention-Gated Neural Routing
//=============================================================================

#ifndef NIMCP_THALAMIC_ROUTER_H
#define NIMCP_THALAMIC_ROUTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_thalamic_router.h
 * @brief Thalamic-style attention-gated signal routing
 *
 * WHAT: Dynamic routing of neural signals based on attention and priority
 * WHY:  The thalamus gates information flow to cortex based on relevance
 * HOW:  Attention-weighted routing with priority queues and multi-destination broadcast
 *
 * BIOLOGICAL BASIS:
 * - Thalamus acts as "gateway to cortex" (Sherman & Guillery, 2001)
 * - Thalamic reticular nucleus (TRN) provides attention-based gating
 * - First-order relay (sensory) vs. higher-order relay (cortico-cortical)
 * - Burst vs. tonic firing modes for different information types
 * - Pulvinar coordinates attention across cortical areas
 *
 * ALGORITHMS:
 * - Attention-weighted signal modulation (0-1 scaling)
 * - Priority-based routing (high priority bypasses queue)
 * - Multi-destination broadcasting (fan-out)
 * - Route learning via Hebbian strengthening
 * - Dynamic routing table with conflict resolution
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define THALAMIC_MAX_DESTINATIONS 16         // Max fan-out per source
#define THALAMIC_MAX_QUEUE_SIZE 16384        // Signal queue capacity (bumped 1000→16384 post 2026-04-23: 5 producers × multimodal training saturated 1000)
#define THALAMIC_PRIORITY_HIGH 3             // High priority level
#define THALAMIC_PRIORITY_NORMAL 2           // Normal priority
#define THALAMIC_PRIORITY_LOW 1              // Low priority

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Signal priority level
 */
typedef enum {
    SIGNAL_PRIORITY_LOW = THALAMIC_PRIORITY_LOW,
    SIGNAL_PRIORITY_NORMAL = THALAMIC_PRIORITY_NORMAL,
    SIGNAL_PRIORITY_HIGH = THALAMIC_PRIORITY_HIGH
} signal_priority_t;

/**
 * @brief Routed signal packet
 */
typedef struct {
    uint32_t source_id;          // Source module/region
    uint32_t* dest_ids;          // Destination IDs
    uint32_t num_dests;          // Number of destinations
    float* signal_data;          // Signal payload
    uint32_t signal_size;        // Payload size
    float attention_weight;      // Attention modulation [0.0, 1.0]
    signal_priority_t priority;  // Routing priority
    uint64_t timestamp_ms;       // Signal timestamp
    bool bypass_queue;           // Skip queue (critical signals)
} routed_signal_t;

/**
 * @brief Routing statistics
 */
typedef struct {
    uint64_t signals_routed;     // Total signals routed
    uint64_t signals_dropped;    // Signals dropped (queue full)
    uint64_t signals_bypassed;   // High priority bypasses
    float avg_latency_ms;        // Average routing latency
    float throughput_hz;         // Signals per second
    uint32_t queue_depth;        // Current queue occupancy
} routing_stats_t;

/**
 * @brief Thalamic router configuration
 */
typedef struct {
    uint32_t max_queue_size;           // Maximum queued signals
    uint32_t max_destinations;         // Max fan-out per route
    bool enable_attention_gating;      // Apply attention weights
    bool enable_priority_routing;      // Use priority queues
    bool enable_statistics;            // Track routing metrics
    float min_attention_threshold;     // Drop signals below threshold
    bool enable_learning;              // Hebbian route strengthening
    bool enable_second_messengers;     // Enable neuromodulator cascades
    uint32_t num_neurons;              // Number of neurons for cascade tracking
    bool enable_quantum_routing;       // Enable quantum attention for O(√N) routing
} thalamic_router_config_t;

/**
 * @brief Opaque thalamic router handle
 */
typedef struct thalamic_router thalamic_router_t;

/**
 * @brief Signal delivery callback
 *
 * WHAT: User-defined function called when signal arrives at destination
 * WHY:  Allow custom processing of routed signals
 * HOW:  Called by router for each destination
 *
 * @param dest_id Destination identifier
 * @param signal Signal data
 * @param size Signal size
 * @param attention Attention weight applied
 * @param user_data User context pointer
 */
typedef void (*signal_delivery_callback_t)(uint32_t dest_id,
                                           const float* signal,
                                           uint32_t size,
                                           float attention,
                                           void* user_data);

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create thalamic router with configuration
 *
 * WHAT: Initialize attention-gated routing system
 * WHY:  Set up routing infrastructure for signal distribution
 * HOW:  Allocate queues, routing tables, and statistics
 *
 * @param config Router configuration (NULL for defaults)
 * @return Router handle or NULL on failure
 */
thalamic_router_t* thalamic_router_create(const thalamic_router_config_t* config);

/**
 * @brief Destroy thalamic router and free resources
 */
void thalamic_router_destroy(thalamic_router_t* router);

/**
 * @brief Route signal to destination(s)
 *
 * WHAT: Send signal through routing system
 * WHY:  Distribute information based on attention and priority
 * HOW:  Apply attention gating, queue or bypass, invoke delivery callbacks
 *
 * @param router Router handle
 * @param signal Signal packet to route
 * @return true on success, false on error (queue full, invalid dest, etc.)
 */
bool thalamic_router_route_signal(thalamic_router_t* router,
                                   const routed_signal_t* signal);

/**
 * @brief Process queued signals (call periodically)
 *
 * WHAT: Deliver queued signals in priority order
 * WHY:  Implement asynchronous routing with priority scheduling
 * HOW:  Dequeue highest priority signals, invoke callbacks
 *
 * @param router Router handle
 * @param max_signals Maximum signals to process this call
 * @param num_processed Output: number of signals processed
 * @return true on success, false on error
 */
bool thalamic_router_process_queue(thalamic_router_t* router,
                                    uint32_t max_signals,
                                    uint32_t* num_processed);

/**
 * @brief Set delivery callback for destination
 *
 * WHAT: Register callback for signal reception
 * WHY:  Allow modules to receive routed signals
 * HOW:  Store callback and context, invoke on signal arrival
 *
 * @param router Router handle
 * @param dest_id Destination identifier
 * @param callback Delivery callback function
 * @param user_data User context pointer
 * @return true on success, false on error
 */
bool thalamic_router_set_callback(thalamic_router_t* router,
                                   uint32_t dest_id,
                                   signal_delivery_callback_t callback,
                                   void* user_data);

/**
 * @brief Set attention weight for route
 *
 * WHAT: Modulate signal strength for specific source-dest pair
 * WHY:  Implement top-down attention control
 * HOW:  Store weight, apply to signals on this route
 *
 * @param router Router handle
 * @param source_id Source module ID
 * @param dest_id Destination module ID
 * @param attention Attention weight [0.0, 1.0]
 * @return true on success, false on error
 */
bool thalamic_router_set_attention(thalamic_router_t* router,
                                    uint32_t source_id,
                                    uint32_t dest_id,
                                    float attention);

/**
 * @brief Get attention weight for route
 *
 * @param router Router handle
 * @param source_id Source module ID
 * @param dest_id Destination module ID
 * @param attention Output: attention weight
 * @return true on success, false if route not found
 */
bool thalamic_router_get_attention(const thalamic_router_t* router,
                                    uint32_t source_id,
                                    uint32_t dest_id,
                                    float* attention);

/**
 * @brief Get routing statistics
 *
 * @param router Router handle
 * @param stats Output: routing statistics
 * @return true on success, false on error
 */
bool thalamic_router_get_stats(const thalamic_router_t* router,
                                routing_stats_t* stats);

/**
 * @brief Reset routing statistics
 */
void thalamic_router_reset_stats(thalamic_router_t* router);

/**
 * @brief Current queue fill ratio [0.0, 1.0]
 *
 * WHAT: Returns queue_size / queue_capacity.
 * WHY:  Producers can query this and throttle low-priority signals
 *       (backpressure without blocking). Read-only, lock-free.
 * HOW:  Atomic read of queue_size + capacity; returns 0.0 on NULL router.
 *
 * @param router  Router instance (NULL-tolerant, returns 0.0).
 * @return Fill ratio in [0.0, 1.0].
 */
float thalamic_router_queue_usage(const thalamic_router_t* router);

/**
 * @brief Is the router under pressure (queue >= 80% full)?
 *
 * WHY:  Convenience predicate for producers. Low-priority senders should
 *       skip emitting signals when this is true to let the consumer drain.
 *       Hysteresis: returns true at >=80%, false at <70% — avoids flapping.
 * HOW:  Static hysteresis threshold stored per-router; state tracked via
 *       atomic bool so concurrent producers see consistent answer.
 *
 * @param router  Router instance (NULL-tolerant, returns false).
 * @return true when router is overloaded and producers should throttle.
 */
bool thalamic_router_is_under_pressure(const thalamic_router_t* router);

/**
 * @brief Clear signal queue
 *
 * WHAT: Remove all queued signals
 * WHY:  Reset routing state after task change
 * HOW:  Free signal packets, reset queue pointers
 */
void thalamic_router_clear_queue(thalamic_router_t* router);

/**
 * @brief Get default configuration
 */
thalamic_router_config_t thalamic_router_default_config(void);

/**
 * @brief Create signal packet (helper)
 *
 * WHAT: Allocate and initialize routed signal
 * WHY:  Convenience function for signal creation
 * HOW:  Allocate memory, copy data, set defaults
 *
 * @param source_id Source module
 * @param dest_ids Destination array
 * @param num_dests Number of destinations
 * @param signal_data Signal payload
 * @param signal_size Payload size
 * @param priority Signal priority
 * @return Allocated signal or NULL on failure (caller must free)
 */
routed_signal_t* thalamic_router_create_signal(uint32_t source_id,
                                                const uint32_t* dest_ids,
                                                uint32_t num_dests,
                                                const float* signal_data,
                                                uint32_t signal_size,
                                                signal_priority_t priority);

/**
 * @brief Free signal packet (helper)
 */
void thalamic_router_free_signal(routed_signal_t* signal);

/**
 * @brief Trigger receptor activation cascade
 *
 * WHAT: Activate neuromodulator receptor and trigger second messenger cascade
 * WHY:  ACh/NE shift burst<->tonic modes via cAMP/PKA cascades
 * HOW:  Route to appropriate G-protein cascade (Gs, Gi, or Gq)
 *
 * @param router Router handle
 * @param neuron_id Target neuron/thalamic nucleus
 * @param receptor Receptor type (e.g., RECEPTOR_ACETYLCHOLINE_M1)
 * @param occupancy Receptor occupancy [0.0, 1.0]
 * @param timestamp_ms Current simulation time
 * @return true on success, false if second messengers disabled
 */
bool thalamic_router_trigger_receptor(thalamic_router_t* router,
                                       uint32_t neuron_id,
                                       uint32_t receptor,
                                       float occupancy,
                                       uint64_t timestamp_ms);

/**
 * @brief Get second messenger cascade state
 *
 * WHAT: Query current intracellular signaling state
 * WHY:  Inspect PKA, PKC, CaMKII activity for debugging/monitoring
 * HOW:  Forward to second messenger system query
 *
 * @param router Router handle
 * @param neuron_id Target neuron to query
 * @param state Output: cascade state (caller-allocated)
 * @return true on success, false if second messengers disabled or invalid neuron
 */
bool thalamic_router_get_second_messenger_state(const thalamic_router_t* router,
                                                 uint32_t neuron_id,
                                                 void* state);

// ============================================================================
// IMAGINATION ENGINE INTEGRATION
// ============================================================================

/* Forward declaration for callback type */
struct nimcp_tensor;

/**
 * @brief Callback for imagination attention gate updates
 *
 * @param attention_weight Current attention weight [0.0-1.0]
 * @param focus_target Current attention target (may be NULL)
 * @param user_data User context
 */
typedef void (*imagination_attention_gate_callback_t)(
    float attention_weight,
    const struct nimcp_tensor* focus_target,
    void* user_data
);

/**
 * @brief Set imagination attention gating weight
 *
 * WHAT: Controls how much imagination content reaches conscious awareness
 * WHY:  Thalamus gates imagination based on attention focus
 * HOW:  Sets weight [0.0-1.0] and sends BIO_MSG_IMAGINATION_ATTENTION_GATE
 *
 * BIOLOGICAL BASIS:
 * The pulvinar nucleus of thalamus coordinates attention-based gating,
 * allowing focused attention to amplify or suppress imagination content
 * reaching conscious awareness.
 *
 * @param router Router handle
 * @param attention_weight Attention weight for imagination [0.0-1.0]
 * @return true on success, false on error
 */
bool thalamic_router_set_imagination_attention(thalamic_router_t* router,
                                                float attention_weight);

/**
 * @brief Get current imagination attention weight
 *
 * @param router Router handle
 * @return Current attention weight [0.0-1.0], or -1.0 on error
 */
float thalamic_router_get_imagination_attention(const thalamic_router_t* router);

/**
 * @brief Register bio-async handlers for imagination attention gating
 *
 * WHAT: Sets up message handlers for imagination-related routing
 * WHY:  Enable asynchronous communication with imagination engine
 * HOW:  Register handler for BIO_MSG_IMAGINATION_ATTENTION_GATE
 *
 * @param router Router handle
 * @return true on success, false on error
 */
bool thalamic_router_register_imagination_handler(thalamic_router_t* router);

/**
 * @brief Set callback for imagination attention gate updates
 *
 * @param router Router handle
 * @param callback Callback function
 * @param user_data User context for callback
 * @return true on success, false on error
 */
bool thalamic_router_set_imagination_callback(thalamic_router_t* router,
                                               imagination_attention_gate_callback_t callback,
                                               void* user_data);

/**
 * @brief Route imagination content based on attention weight
 *
 * WHAT: Routes imagination scenario content to appropriate destinations
 * WHY:  Prioritizes imagination when attention weight is high
 * HOW:  Creates routed signal with attention-modulated priority
 *
 * BIOLOGICAL BASIS:
 * When attention is focused on imagination (high weight), the thalamus
 * prioritizes routing imagination content over external sensory input.
 * This models the "absorbed in thought" state.
 *
 * @param router Router handle
 * @param scenario_id Imagination scenario identifier
 * @param content Imagination content data
 * @param content_size Size of content in floats
 * @param dest_ids Destination module IDs
 * @param num_dests Number of destinations
 * @return true on success, false on error
 */
bool thalamic_router_route_imagination_content(thalamic_router_t* router,
                                                uint32_t scenario_id,
                                                const float* content,
                                                uint32_t content_size,
                                                const uint32_t* dest_ids,
                                                uint32_t num_dests);

/**
 * @brief Enable or disable imagination routing
 *
 * @param router Router handle
 * @param enabled true to enable, false to disable
 * @return true on success, false on error
 */
bool thalamic_router_set_imagination_routing_enabled(thalamic_router_t* router,
                                                      bool enabled);

/**
 * @brief Check if imagination routing is enabled
 *
 * @param router Router handle
 * @return true if enabled, false otherwise
 */
bool thalamic_router_is_imagination_routing_enabled(const thalamic_router_t* router);

/*=============================================================================
 * Health Agent Integration
 *===========================================================================*/

/**
 * @brief Set health agent for thalamic router heartbeats
 *
 * WHAT: Associate health agent with thalamic router for health monitoring
 * WHY:  Enables heartbeat reporting during signal routing
 * HOW:  Store agent reference, send heartbeats during dispatch
 *
 * @param agent Health agent instance (can be NULL to disable)
 */
struct nimcp_health_agent;
void thalamic_router_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_THALAMIC_ROUTER_H
