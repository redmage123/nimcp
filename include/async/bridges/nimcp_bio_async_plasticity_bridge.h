/**
 * @file nimcp_bio_async_plasticity_bridge.h
 * @brief Bio-Async Plasticity Bridge - Spike Timing Integration
 *
 * WHAT: Bridge between bio-async messaging and plasticity systems
 * WHY:  Enable spike timing information in bio-async messages for STDP
 * HOW:  Converts bio_message_t to plasticity_event_t and vice versa
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SPIKE TIMING IN NEURAL COMMUNICATION:
 * -------------------------------------
 * Spike timing is critical for plasticity (STDP requires sub-ms precision).
 * This bridge enables:
 *
 * 1. Spike Event Propagation:
 *    - Pre/post spike times encoded in bio-async messages
 *    - Timing precision maintained across module boundaries
 *    - Support for batch spike events (efficiency)
 *
 * 2. Plasticity Event Broadcasting:
 *    - LTP/LTD events broadcast to interested modules
 *    - Weight changes propagated to visualization/monitoring
 *    - Consolidation events for memory systems
 *
 * 3. Neuromodulator Integration:
 *    - DA bursts for reward-modulated STDP
 *    - ACh for attention-gated plasticity
 *    - 5-HT for sleep-dependent consolidation
 *    - NE for stress/arousal modulation
 *
 * CHANNEL ASSIGNMENT:
 * -------------------
 * - DOPAMINE: Reward signals, LTP events, weight increases
 * - SEROTONIN: Slow plasticity, consolidation, homeostatic
 * - NOREPINEPHRINE: Urgent plasticity alerts, saturation warnings
 * - ACETYLCHOLINE: Attention-gated plasticity, spike timing queries
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    BIO-ASYNC PLASTICITY BRIDGE                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                     SPIKE TIMING LAYER                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   Pre-spike event ──┬──→ BIO_MSG_STDP_EVENT ───→ Plasticity       │  ║
 * ║   │   Post-spike event ─┘                              Coordinator     │  ║
 * ║   │                                                                     │  ║
 * ║   │   Batch spikes ────────→ BIO_MSG_STDP_BATCH_EVENT ───→ Batch STDP │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   PLASTICITY EVENT LAYER                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   plasticity_event_t ←───→ bio_message_t conversion                │  ║
 * ║   │   Weight changes    ←───→ BIO_MSG_WEIGHT_CHANGE                    │  ║
 * ║   │   LTP/LTD events    ←───→ BIO_MSG_LTP_INDUCED / BIO_MSG_LTD_INDUCED│  ║
 * ║   │   Consolidation     ←───→ BIO_MSG_CONSOLIDATION_TRIGGER            │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  COORDINATOR REGISTRATION                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   Register with plasticity_coordinator_t for unified updates       │  ║
 * ║   │   Subscribe to plasticity events for broadcasting                  │  ║
 * ║   │   Report spike timing to STDP mechanisms                           │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2026-02-03
 */

#ifndef NIMCP_BIO_ASYNC_PLASTICITY_BRIDGE_H
#define NIMCP_BIO_ASYNC_PLASTICITY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"

/* ============================================================================
 * Module Identifier
 * ============================================================================ */

/** Bio-async module ID for plasticity bridge */
#define BIO_MODULE_BIO_ASYNC_PLASTICITY    0x2000

#define BIO_ASYNC_PLASTICITY_MODULE_NAME   "bio_async_plasticity_bridge"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BIO_ASYNC_PLASTICITY_MAX_BATCH_SPIKES     64   /**< Max spikes per batch */
#define BIO_ASYNC_PLASTICITY_MAX_SUBSCRIPTIONS    32   /**< Max event subscriptions */
#define BIO_ASYNC_PLASTICITY_DEFAULT_INBOX_SIZE   128  /**< Default message inbox */

/* ============================================================================
 * Message Payloads
 * ============================================================================ */

/**
 * @brief STDP spike event payload
 *
 * WHAT: Single spike timing event for STDP processing
 * WHY:  Enable precise timing-dependent plasticity across modules
 */
typedef struct bio_async_spike_event {
    uint32_t synapse_id;           /**< Target synapse ID */
    uint32_t pre_neuron_id;        /**< Presynaptic neuron ID */
    uint32_t post_neuron_id;       /**< Postsynaptic neuron ID */
    uint64_t spike_time_us;        /**< Spike time in microseconds */
    bool is_pre_spike;             /**< true=pre-spike, false=post-spike */
    float current_weight;          /**< Current synaptic weight (if available) */
} bio_async_spike_event_t;

/**
 * @brief Batch STDP spike events payload
 *
 * WHAT: Multiple spike events for efficient batch processing
 * WHY:  Reduce messaging overhead for high-frequency spiking
 */
typedef struct bio_async_spike_batch {
    uint32_t num_events;           /**< Number of events in batch */
    uint64_t batch_start_time_us;  /**< Timestamp of first event */
    uint64_t batch_end_time_us;    /**< Timestamp of last event */
    bio_async_spike_event_t events[BIO_ASYNC_PLASTICITY_MAX_BATCH_SPIKES];
} bio_async_spike_batch_t;

/**
 * @brief Weight change notification payload
 *
 * WHAT: Notification of synaptic weight modification
 * WHY:  Allow monitoring and visualization of plasticity
 */
typedef struct bio_async_weight_change {
    uint32_t synapse_id;           /**< Affected synapse */
    uint32_t pre_neuron_id;        /**< Presynaptic neuron */
    uint32_t post_neuron_id;       /**< Postsynaptic neuron */
    float old_weight;              /**< Weight before change */
    float new_weight;              /**< Weight after change */
    float delta;                   /**< Change amount */
    plasticity_event_type_t cause; /**< What caused the change */
    uint64_t timestamp_us;         /**< When change occurred */
} bio_async_weight_change_t;

/**
 * @brief Plasticity event broadcast payload
 *
 * WHAT: Generic plasticity event for broadcasting
 * WHY:  Unified event format for cross-module communication
 */
typedef struct bio_async_plasticity_event {
    plasticity_event_type_t type;  /**< Event type (LTP, LTD, etc.) */
    uint32_t synapse_id;           /**< Affected synapse (if applicable) */
    uint32_t neuron_id;            /**< Affected neuron (if applicable) */
    float old_value;               /**< Value before event */
    float new_value;               /**< Value after event */
    float delta;                   /**< Change amount */
    uint64_t timestamp_us;         /**< Event timestamp */
    void* context;                 /**< Optional context data */
} bio_async_plasticity_event_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bio-async plasticity bridge configuration
 */
typedef struct bio_async_plasticity_config {
    /* Channel assignment */
    nimcp_bio_channel_type_t spike_channel;      /**< Channel for spike events (ACh) */
    nimcp_bio_channel_type_t weight_channel;     /**< Channel for weight changes (DA) */
    nimcp_bio_channel_type_t alert_channel;      /**< Channel for alerts (NE) */
    nimcp_bio_channel_type_t consolidation_channel; /**< Channel for consolidation (5-HT) */

    /* Batching */
    bool enable_batch_mode;                      /**< Batch spike events */
    uint32_t batch_size_threshold;               /**< Spikes before batch send */
    uint32_t batch_time_threshold_us;            /**< Max time before batch send */

    /* Filtering */
    float min_weight_change_notify;              /**< Min change to broadcast */
    bool broadcast_all_events;                   /**< Broadcast all plasticity events */

    /* Integration */
    bool connect_to_coordinator;                 /**< Auto-connect to coordinator */
    bool register_spike_handlers;                /**< Register for spike messages */
    uint32_t inbox_capacity;                     /**< Message inbox size */

    /* Priority */
    uint8_t spike_priority;                      /**< Priority for spike events */
    uint8_t weight_priority;                     /**< Priority for weight updates */
    uint8_t alert_priority;                      /**< Priority for alerts */
} bio_async_plasticity_config_t;

/* ============================================================================
 * Subscription
 * ============================================================================ */

/**
 * @brief Plasticity event subscription
 */
typedef struct bio_async_plasticity_subscription {
    plasticity_event_type_t event_type;          /**< Event type to subscribe */
    void (*callback)(const bio_async_plasticity_event_t* event, void* user_data);
    void* user_data;                             /**< User context */
    bool active;                                 /**< Subscription active */
} bio_async_plasticity_subscription_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct bio_async_plasticity_stats {
    /* Spike events */
    uint64_t spikes_received;                    /**< Total spikes received */
    uint64_t spikes_sent;                        /**< Total spikes sent */
    uint64_t batches_processed;                  /**< Batch events processed */

    /* Plasticity events */
    uint64_t ltp_events;                         /**< LTP events broadcast */
    uint64_t ltd_events;                         /**< LTD events broadcast */
    uint64_t weight_changes_broadcast;           /**< Weight change notifications */
    uint64_t consolidation_events;               /**< Consolidation triggers */

    /* Messaging */
    uint64_t messages_sent;                      /**< Total messages sent */
    uint64_t messages_received;                  /**< Total messages received */
    uint64_t messages_dropped;                   /**< Messages dropped (queue full) */

    /* Timing */
    uint64_t avg_spike_latency_us;               /**< Avg spike processing latency */
    uint64_t max_spike_latency_us;               /**< Max spike processing latency */
} bio_async_plasticity_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Bio-async plasticity bridge
 *
 * Central hub for spike timing and plasticity event messaging
 */
typedef struct bio_async_plasticity_bridge {
    /* Base bridge (MUST be first) */
    bridge_base_t base;

    /* Configuration */
    bio_async_plasticity_config_t config;

    /* Connections */
    plasticity_orchestrator_t* orchestrator;     /**< Plasticity orchestrator */
    bio_router_t* router;                        /**< Bio-async router */

    /* Subscriptions */
    bio_async_plasticity_subscription_t* subscriptions;
    uint32_t num_subscriptions;
    uint32_t max_subscriptions;

    /* Batch accumulator */
    bio_async_spike_batch_t pending_batch;       /**< Accumulated spikes */
    uint64_t batch_start_time_us;                /**< When batch started */

    /* Statistics */
    bio_async_plasticity_stats_t stats;

    /* State */
    bool connected;                              /**< Connected to router */
    bool coordinator_connected;                  /**< Connected to coordinator */
    bool paused;                                 /**< Message processing paused */
    uint64_t creation_time_us;                   /**< Bridge creation timestamp */
} bio_async_plasticity_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide biologically-plausible default parameters
 * WHY:  Easy initialization with sensible values
 * HOW:  Set channel assignments and timing defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_default_config(bio_async_plasticity_config_t* config);

/**
 * @brief Create bio-async plasticity bridge
 *
 * WHAT: Initialize bridge for spike timing integration
 * WHY:  Enable plasticity events in bio-async messaging
 * HOW:  Allocate structures, connect to coordinator
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
bio_async_plasticity_bridge_t* bio_async_plasticity_bridge_create(
    const bio_async_plasticity_config_t* config
);

/**
 * @brief Destroy bio-async plasticity bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void bio_async_plasticity_bridge_destroy(bio_async_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset statistics and pending batches
 * WHY:  Start fresh without recreating
 * HOW:  Clear accumulators, reset stats
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_bridge_reset(bio_async_plasticity_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async for messaging
 * WHY:  Enable spike timing message exchange
 * HOW:  Register module, set up handlers
 *
 * @param bridge Bridge to connect
 * @param router Bio-async router (NULL for global)
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_bridge_connect(
    bio_async_plasticity_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int bio_async_plasticity_bridge_disconnect(bio_async_plasticity_bridge_t* bridge);

/**
 * @brief Check if connected
 *
 * @param bridge Bridge to check
 * @return true if connected to router
 */
bool bio_async_plasticity_bridge_is_connected(const bio_async_plasticity_bridge_t* bridge);

/**
 * @brief Connect to plasticity coordinator
 *
 * WHAT: Link bridge to plasticity coordinator
 * WHY:  Enable unified plasticity management
 * HOW:  Register event callbacks with coordinator
 *
 * @param bridge Bridge
 * @param orchestrator Plasticity orchestrator
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_connect_coordinator(
    bio_async_plasticity_bridge_t* bridge,
    plasticity_orchestrator_t* orchestrator
);

/**
 * @brief Disconnect from plasticity coordinator
 *
 * @param bridge Bridge
 * @return 0 on success
 */
int bio_async_plasticity_disconnect_coordinator(bio_async_plasticity_bridge_t* bridge);

/* ============================================================================
 * Spike Event API
 * ============================================================================ */

/**
 * @brief Send spike event
 *
 * WHAT: Broadcast spike timing to plasticity systems
 * WHY:  Enable STDP across module boundaries
 * HOW:  Send via bio-async or batch if enabled
 *
 * @param bridge Bridge
 * @param event Spike event to send
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_send_spike(
    bio_async_plasticity_bridge_t* bridge,
    const bio_async_spike_event_t* event
);

/**
 * @brief Send batch of spike events
 *
 * WHAT: Send multiple spikes efficiently
 * WHY:  Reduce messaging overhead
 * HOW:  Pack spikes into batch message
 *
 * @param bridge Bridge
 * @param batch Spike batch to send
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_send_batch(
    bio_async_plasticity_bridge_t* bridge,
    const bio_async_spike_batch_t* batch
);

/**
 * @brief Flush pending batch
 *
 * WHAT: Send accumulated spikes immediately
 * WHY:  Force batch transmission before timeout
 * HOW:  Send current batch, reset accumulator
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_flush_batch(bio_async_plasticity_bridge_t* bridge);

/* ============================================================================
 * Plasticity Event API
 * ============================================================================ */

/**
 * @brief Broadcast plasticity event
 *
 * WHAT: Send plasticity event to interested modules
 * WHY:  Enable cross-module plasticity coordination
 * HOW:  Convert to bio-message and send
 *
 * @param bridge Bridge
 * @param event Plasticity event to broadcast
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_broadcast_event(
    bio_async_plasticity_bridge_t* bridge,
    const plasticity_event_t* event
);

/**
 * @brief Broadcast weight change
 *
 * WHAT: Notify of synaptic weight modification
 * WHY:  Allow monitoring and visualization
 * HOW:  Send weight change message on DA channel
 *
 * @param bridge Bridge
 * @param change Weight change details
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_broadcast_weight_change(
    bio_async_plasticity_bridge_t* bridge,
    const bio_async_weight_change_t* change
);

/* ============================================================================
 * Subscription API
 * ============================================================================ */

/**
 * @brief Subscribe to plasticity events
 *
 * WHAT: Register callback for specific event types
 * WHY:  Allow modules to react to plasticity
 * HOW:  Add to subscription list
 *
 * @param bridge Bridge
 * @param event_type Event type to subscribe
 * @param callback Callback function
 * @param user_data User context
 * @return Subscription ID (>=0) or -1 on error
 */
int bio_async_plasticity_subscribe(
    bio_async_plasticity_bridge_t* bridge,
    plasticity_event_type_t event_type,
    void (*callback)(const bio_async_plasticity_event_t* event, void* user_data),
    void* user_data
);

/**
 * @brief Unsubscribe from plasticity events
 *
 * @param bridge Bridge
 * @param subscription_id ID from subscribe
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_unsubscribe(
    bio_async_plasticity_bridge_t* bridge,
    int subscription_id
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int bio_async_plasticity_get_stats(
    const bio_async_plasticity_bridge_t* bridge,
    bio_async_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 */
void bio_async_plasticity_reset_stats(bio_async_plasticity_bridge_t* bridge);

/* ============================================================================
 * Control API
 * ============================================================================ */

/**
 * @brief Pause message processing
 *
 * @param bridge Bridge
 */
void bio_async_plasticity_pause(bio_async_plasticity_bridge_t* bridge);

/**
 * @brief Resume message processing
 *
 * @param bridge Bridge
 */
void bio_async_plasticity_resume(bio_async_plasticity_bridge_t* bridge);

/**
 * @brief Check if paused
 *
 * @param bridge Bridge
 * @return true if paused
 */
bool bio_async_plasticity_is_paused(const bio_async_plasticity_bridge_t* bridge);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void bio_async_plasticity_set_health_agent(void* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_ASYNC_PLASTICITY_BRIDGE_H */
