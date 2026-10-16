/**
 * @file nimcp_core_bio_async_bridge.h
 * @brief Core Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for all core NIMCP modules including brain,
 *       medulla, neuron types, topology, and neural logic.
 *
 * WHY: The core modules form the foundation of NIMCP's neural computation system.
 *      This bridge enables:
 *      - Registration of all core modules with the bio-async router
 *      - Standardized message types for inter-core communication
 *      - Brain-wide state coordination and synchronization
 *      - Efficient neuron-to-neuron spike routing
 *      - Module lifecycle coordination
 *
 * HOW: Registers core modules with bio-router, maintains module registry,
 *      provides typed message APIs, and routes spike events between neurons.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * NEURAL COMMUNICATION PATHWAYS:
 * ------------------------------
 * 1. Fast Spiking Communication (Glutamatergic):
 *    - Action potentials propagate along axons
 *    - Synaptic transmission at millisecond timescales
 *    - Mapped to: ACETYLCHOLINE channel (fast attention)
 *
 * 2. Modulatory Communication (Neuromodulatory):
 *    - Volume transmission of dopamine, serotonin, etc.
 *    - Slower, more diffuse effects
 *    - Mapped to: DOPAMINE/SEROTONIN channels
 *
 * 3. State Coordination (Oscillatory):
 *    - Brain-wide oscillations synchronize activity
 *    - Phase coupling between regions
 *    - Mapped to: Phase sync API
 *
 * 4. Metabolic Signaling (Glial):
 *    - Calcium waves coordinate resource allocation
 *    - System-wide state changes
 *    - Mapped to: Glial wave API
 *
 * MESSAGE ROUTING ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    CORE BIO-ASYNC BRIDGE                                   |
 * +===========================================================================+
 * |                                                                            |
 * |   REGISTERED CORE MODULES                                                  |
 * |   -----------------------                                                  |
 * |   +--------+  +--------+  +--------+  +--------+  +--------+              |
 * |   | Brain  |  | Medulla|  | Neuron |  | Topo-  |  | Neural |              |
 * |   | Module |  | Module |  | Types  |  | logy   |  | Logic  |              |
 * |   +---+----+  +---+----+  +---+----+  +---+----+  +---+----+              |
 * |       |           |           |           |           |                    |
 * |       +-----+-----+-----+-----+-----+-----+-----+-----+                    |
 * |             |                                                              |
 * |             v                                                              |
 * |   +-------------------------------------------------------------------+   |
 * |   |                     BIO-ASYNC ROUTER                               |   |
 * |   +-------------------------------------------------------------------+   |
 * |             |                                                              |
 * |             v                                                              |
 * |   +-------------------------------------------------------------------+   |
 * |   |                   SPIKE ROUTING ENGINE                             |   |
 * |   |  - Pre-synaptic neuron -> Post-synaptic targets                   |   |
 * |   |  - Fanout management for high-connectivity neurons                |   |
 * |   |  - Priority routing for inhibitory interneurons                   |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - Uses bridge_base_t for common infrastructure
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORE_BIO_ASYNC_BRIDGE_H
#define NIMCP_CORE_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes (Core Bio-Async specific)
 * ============================================================================ */

#define CORE_BIO_ERROR_BASE                     11000
#define CORE_BIO_ERROR_NOT_INITIALIZED          (CORE_BIO_ERROR_BASE + 1)
#define CORE_BIO_ERROR_ALREADY_INITIALIZED      (CORE_BIO_ERROR_BASE + 2)
#define CORE_BIO_ERROR_MODULE_NOT_FOUND         (CORE_BIO_ERROR_BASE + 3)
#define CORE_BIO_ERROR_MODULE_ALREADY_REGISTERED (CORE_BIO_ERROR_BASE + 4)
#define CORE_BIO_ERROR_INVALID_MODULE_TYPE      (CORE_BIO_ERROR_BASE + 5)
#define CORE_BIO_ERROR_SPIKE_ROUTING_FAILED     (CORE_BIO_ERROR_BASE + 6)
#define CORE_BIO_ERROR_STATE_SYNC_FAILED        (CORE_BIO_ERROR_BASE + 7)
#define CORE_BIO_ERROR_MAX_MODULES_EXCEEDED     (CORE_BIO_ERROR_BASE + 8)
#define CORE_BIO_ERROR_HANDLER_FAILED           (CORE_BIO_ERROR_BASE + 9)
#define CORE_BIO_ERROR_ROUTER_NOT_AVAILABLE     (CORE_BIO_ERROR_BASE + 10)

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of registered core modules */
#define CORE_BIO_MAX_MODULES                    32

/** Maximum pending spikes in routing queue */
#define CORE_BIO_MAX_PENDING_SPIKES             4096

/** Maximum spike fanout (post-synaptic targets) */
#define CORE_BIO_MAX_SPIKE_FANOUT               256

/** Default spike routing batch size */
#define CORE_BIO_DEFAULT_SPIKE_BATCH            64

/** Message expiry time (ms) */
#define CORE_BIO_MESSAGE_TTL_MS                 5000

/** Brain state broadcast interval (ms) */
#define CORE_BIO_STATE_BROADCAST_INTERVAL_MS    100

/** High-priority spike threshold (inhibitory neurons) */
#define CORE_BIO_INHIBITORY_PRIORITY_THRESHOLD  0.8f

/* ============================================================================
 * Message Type Enumeration (Core-specific)
 * ============================================================================ */

/**
 * @brief Core module bio-async message types
 *
 * WHAT: Message type enumeration for core module bio-async routing
 * WHY:  Enables typed message handling for neural computation coordination
 * HOW:  Each type corresponds to a specific core module operation
 */
typedef enum {
    /* Brain state messages */
    CORE_MSG_BRAIN_STATE = 0,           /**< Complete brain state broadcast */
    CORE_MSG_BRAIN_STATE_QUERY,         /**< Query brain state */
    CORE_MSG_BRAIN_CONFIG_UPDATE,       /**< Brain configuration changed */
    CORE_MSG_BRAIN_REGION_ACTIVATED,    /**< Brain region activated */

    /* Neuron spike messages */
    CORE_MSG_NEURON_SPIKE,              /**< Single neuron spike event */
    CORE_MSG_NEURON_SPIKE_BATCH,        /**< Batch of spike events */
    CORE_MSG_SPIKE_ROUTE_REQUEST,       /**< Request spike routing */
    CORE_MSG_SPIKE_ROUTE_COMPLETE,      /**< Spike routing completed */

    /* Medulla messages */
    CORE_MSG_MEDULLA_STATE,             /**< Medulla state update */
    CORE_MSG_MEDULLA_AROUSAL,           /**< Arousal level change */
    CORE_MSG_MEDULLA_PROTECTION,        /**< Protection level change */
    CORE_MSG_MEDULLA_CIRCADIAN,         /**< Circadian phase update */

    /* Neuron type messages */
    CORE_MSG_NEURON_TYPE_REGISTER,      /**< Neuron type registration */
    CORE_MSG_NEURON_TYPE_QUERY,         /**< Query neuron type info */
    CORE_MSG_NEURON_TYPE_UPDATE,        /**< Neuron type parameters changed */

    /* Topology messages */
    CORE_MSG_TOPOLOGY_UPDATE,           /**< Network topology changed */
    CORE_MSG_TOPOLOGY_QUERY,            /**< Query topology info */
    CORE_MSG_CONNECTION_ADDED,          /**< New connection added */
    CORE_MSG_CONNECTION_REMOVED,        /**< Connection removed */

    /* Neural logic messages */
    CORE_MSG_LOGIC_GATE_UPDATE,         /**< Logic gate state changed */
    CORE_MSG_LOGIC_CIRCUIT_STEP,        /**< Logic circuit stepped */

    /* Coordination messages */
    CORE_MSG_SYNC_REQUEST,              /**< Request synchronization */
    CORE_MSG_SYNC_COMPLETE,             /**< Synchronization completed */
    CORE_MSG_HEALTH_CHECK,              /**< Health check request */
    CORE_MSG_HEALTH_RESPONSE,           /**< Health check response */

    CORE_MSG_TYPE_COUNT
} core_bio_msg_type_t;

/* ============================================================================
 * Core Module Type Enumeration
 * ============================================================================ */

/**
 * @brief Core module types for registration
 */
typedef enum {
    CORE_MODULE_BRAIN = 0,              /**< Main brain module */
    CORE_MODULE_MEDULLA,                /**< Medulla oblongata */
    CORE_MODULE_NEURON_TYPES,           /**< Neuron type registry */
    CORE_MODULE_TOPOLOGY,               /**< Network topology */
    CORE_MODULE_NEURAL_LOGIC,           /**< Neural logic gates */
    CORE_MODULE_NEURALNET,              /**< Neural network module */
    CORE_MODULE_AXON,                   /**< Axon module */
    CORE_MODULE_SYNAPSE,                /**< Synapse module */
    CORE_MODULE_CORTICAL_COLUMN,        /**< Cortical column module */

    CORE_MODULE_TYPE_COUNT
} core_module_type_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Brain state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Brain state */
    uint32_t active_neurons;            /**< Number of currently active neurons */
    uint32_t total_neurons;             /**< Total neuron count */
    uint32_t active_synapses;           /**< Number of active synapses */
    float global_activity_level;        /**< Overall activity [0, 1] */

    /* Oscillation state */
    float dominant_frequency_hz;        /**< Dominant oscillation frequency */
    float phase_coherence;              /**< Phase coherence [0, 1] */
    nimcp_oscillation_band_t dominant_band; /**< Dominant oscillation band */

    /* Resource usage */
    float cpu_utilization;              /**< CPU usage [0, 1] */
    size_t memory_bytes;                /**< Memory usage in bytes */

    /* Timing */
    uint64_t simulation_time_us;        /**< Simulation time in microseconds */
    uint64_t timestamp_us;              /**< Real timestamp */
} core_bio_brain_state_msg_t;

/**
 * @brief Single neuron spike message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Neuron identification */
    uint32_t neuron_id;                 /**< Pre-synaptic neuron ID */
    uint32_t layer_id;                  /**< Layer containing neuron */
    uint32_t region_id;                 /**< Brain region ID */

    /* Spike characteristics */
    float spike_amplitude;              /**< Spike amplitude (mV) */
    float membrane_potential;           /**< Membrane potential at spike */
    uint64_t spike_time_us;             /**< Spike timestamp */

    /* Neuron type info */
    uint32_t neuron_type;               /**< Neuron type ID */
    bool is_inhibitory;                 /**< True if inhibitory neuron */

    /* Routing hints */
    uint32_t fanout_count;              /**< Number of post-synaptic targets */
    bool high_priority;                 /**< Priority routing flag */
} core_bio_spike_msg_t;

/**
 * @brief Batch spike message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t spike_count;               /**< Number of spikes in batch */
    uint64_t batch_start_time_us;       /**< Batch start timestamp */
    uint64_t batch_end_time_us;         /**< Batch end timestamp */

    /* Spike data (variable length) */
    uint32_t neuron_ids[CORE_BIO_DEFAULT_SPIKE_BATCH];
    float amplitudes[CORE_BIO_DEFAULT_SPIKE_BATCH];
    uint64_t times_us[CORE_BIO_DEFAULT_SPIKE_BATCH];
} core_bio_spike_batch_msg_t;

/**
 * @brief Medulla state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* State levels */
    float arousal_level;                /**< Arousal level [0, 1] */
    float protection_level;             /**< Protection level [0, 1] */
    float circadian_phase;              /**< Circadian phase [0, 2*PI] */

    /* State enums */
    uint32_t arousal_state;             /**< Arousal level enum */
    uint32_t protection_state;          /**< Protection level enum */
    uint32_t medulla_state;             /**< Overall medulla state */

    /* Flags */
    bool emergency_active;              /**< Emergency state active */
    bool sleep_mode;                    /**< Sleep mode active */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} core_bio_medulla_msg_t;

/**
 * @brief Topology update message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t connection_count;          /**< Total connection count */
    uint32_t neuron_count;              /**< Total neuron count */
    uint32_t layer_count;               /**< Number of layers */

    /* Change info */
    uint32_t connections_added;         /**< Connections added since last */
    uint32_t connections_removed;       /**< Connections removed since last */
    uint32_t neurons_added;             /**< Neurons added since last */

    uint64_t timestamp_us;              /**< Update timestamp */
} core_bio_topology_msg_t;

/**
 * @brief Spike routing request payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t source_neuron_id;          /**< Source neuron */
    float spike_amplitude;              /**< Spike amplitude */
    uint64_t spike_time_us;             /**< Spike timestamp */

    /* Target specification (optional) */
    uint32_t target_count;              /**< Number of explicit targets (0 = use topology) */
    uint32_t target_neurons[CORE_BIO_MAX_SPIKE_FANOUT]; /**< Target neuron IDs */
    float synaptic_weights[CORE_BIO_MAX_SPIKE_FANOUT];  /**< Synaptic weights */
} core_bio_spike_route_msg_t;

/**
 * @brief Health check response payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    core_module_type_t module_type;     /**< Module type */
    bool is_healthy;                    /**< Health status */
    float health_score;                 /**< Health score [0, 1] */
    uint64_t uptime_ms;                 /**< Module uptime */
    uint64_t messages_processed;        /**< Messages processed */
    uint64_t errors_encountered;        /**< Error count */

    uint64_t timestamp_us;              /**< Response timestamp */
} core_bio_health_msg_t;

/* ============================================================================
 * Module Registration Entry
 * ============================================================================ */

/**
 * @brief Core module registration entry
 */
typedef struct {
    core_module_type_t type;            /**< Module type */
    bio_module_id_t bio_module_id;      /**< Bio-router module ID */
    const char* name;                   /**< Module name */
    void* module_ptr;                   /**< Pointer to module instance */
    bio_module_context_t bio_ctx;       /**< Bio-async context */
    bool active;                        /**< Registration active */
    uint64_t registration_time;         /**< When registered */
    uint64_t messages_sent;             /**< Messages sent count */
    uint64_t messages_received;         /**< Messages received count */
} core_module_entry_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Core bio-async bridge configuration
 */
typedef struct {
    /* Spike routing */
    uint32_t max_pending_spikes;        /**< Maximum pending spikes */
    uint32_t spike_batch_size;          /**< Spike batch size */
    bool enable_priority_routing;       /**< Enable inhibitory priority */
    float priority_threshold;           /**< Inhibitory priority threshold */

    /* State broadcasts */
    uint32_t state_broadcast_interval_ms; /**< State broadcast interval */
    bool enable_auto_broadcast;         /**< Auto-broadcast state */

    /* Message handling */
    uint32_t max_inbox_process;         /**< Max inbox messages per update */
    uint32_t message_ttl_ms;            /**< Message time-to-live */

    /* Channel configuration */
    nimcp_bio_channel_type_t spike_channel;   /**< Channel for spike messages */
    nimcp_bio_channel_type_t state_channel;   /**< Channel for state messages */
    nimcp_bio_channel_type_t urgent_channel;  /**< Channel for urgent messages */

    /* Feature flags */
    bool enable_spike_routing;          /**< Enable spike routing engine */
    bool enable_topology_sync;          /**< Enable topology synchronization */
    bool enable_health_monitoring;      /**< Enable health check system */
    bool enable_logging;                /**< Enable message logging */
} core_bio_async_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;             /**< Total messages sent */
    uint64_t messages_received;         /**< Total messages received */
    uint64_t messages_dropped;          /**< Messages dropped */
    uint64_t broadcasts_sent;           /**< Broadcast messages sent */

    /* Spike routing stats */
    uint64_t spikes_routed;             /**< Total spikes routed */
    uint64_t spikes_dropped;            /**< Spikes dropped (queue full) */
    uint64_t spike_batches_processed;   /**< Spike batches processed */
    float avg_spike_fanout;             /**< Average spike fanout */
    float avg_routing_latency_us;       /**< Average routing latency */

    /* Per-message-type counts */
    uint64_t brain_state_broadcasts;    /**< Brain state broadcasts */
    uint64_t medulla_state_broadcasts;  /**< Medulla state broadcasts */
    uint64_t topology_updates;          /**< Topology updates sent */
    uint64_t health_checks;             /**< Health checks performed */

    /* Module stats */
    uint32_t registered_modules;        /**< Currently registered modules */
    uint32_t peak_modules;              /**< Peak module count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;    /**< Last broadcast timestamp */
    float max_routing_latency_us;       /**< Peak routing latency */

    /* Error counts */
    uint64_t handler_errors;            /**< Message handler errors */
    uint64_t routing_errors;            /**< Spike routing errors */
} core_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure (Opaque)
 * ============================================================================ */

/**
 * @brief Core bio-async bridge handle
 */
typedef struct core_bio_async_bridge_struct core_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for core bio-async bridge configuration
 * WHY:  Easy initialization with biologically-realistic parameters
 * HOW:  Return struct with evidence-based timing and thresholds
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int core_bio_async_default_config(core_bio_async_config_t* config);

/**
 * @brief Create core bio-async bridge
 *
 * WHAT: Initialize bio-async integration for core modules
 * WHY:  Enable message routing between all core NIMCP modules
 * HOW:  Allocate structure, initialize registry, prepare handlers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
core_bio_async_bridge_t* core_bio_async_bridge_create(
    const core_bio_async_config_t* config
);

/**
 * @brief Destroy core bio-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect modules, free registry, release memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void core_bio_async_bridge_destroy(core_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-router
 *
 * WHAT: Establish connection to the bio-async router
 * WHY:  Enable message routing through central infrastructure
 * HOW:  Register bridge module, set up core handlers
 *
 * @param bridge Bio-async bridge
 * @param router Bio-router (NULL to use global)
 * @return 0 on success, -1 on error
 */
int core_bio_async_connect(
    core_bio_async_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Disconnect from bio-router
 * WHY:  Clean disconnection before shutdown
 * HOW:  Unregister all modules, clear handlers
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int core_bio_async_disconnect(core_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bio-async bridge
 * @return true if connected to router
 */
bool core_bio_async_is_connected(const core_bio_async_bridge_t* bridge);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register a core module with the bridge
 *
 * WHAT: Register a core module for bio-async messaging
 * WHY:  Enable the module to send/receive messages via the bridge
 * HOW:  Add entry to module registry, register with bio-router
 *
 * @param bridge Bio-async bridge
 * @param type Module type
 * @param name Module name
 * @param module_ptr Pointer to module instance
 * @return 0 on success, error code on failure
 */
int core_bio_async_register_module(
    core_bio_async_bridge_t* bridge,
    core_module_type_t type,
    const char* name,
    void* module_ptr
);

/**
 * @brief Unregister a core module from the bridge
 *
 * WHAT: Remove module from bio-async messaging
 * WHY:  Clean module removal before shutdown
 * HOW:  Remove from registry, unregister from bio-router
 *
 * @param bridge Bio-async bridge
 * @param type Module type to unregister
 * @return 0 on success, error code on failure
 */
int core_bio_async_unregister_module(
    core_bio_async_bridge_t* bridge,
    core_module_type_t type
);

/**
 * @brief Get registered module info
 *
 * @param bridge Bio-async bridge
 * @param type Module type
 * @param entry Output entry (can be NULL to just check registration)
 * @return true if module is registered
 */
bool core_bio_async_get_module(
    const core_bio_async_bridge_t* bridge,
    core_module_type_t type,
    core_module_entry_t* entry
);

/**
 * @brief Get count of registered modules
 *
 * @param bridge Bio-async bridge
 * @return Number of registered modules
 */
uint32_t core_bio_async_get_module_count(const core_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Process pending messages from bio-router inbox
 * WHY:  Handle incoming requests and updates from other modules
 * HOW:  Pop messages, dispatch to appropriate handlers
 *
 * @param bridge Bio-async bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int core_bio_async_process_inbox(
    core_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Perform periodic bridge updates
 * WHY:  Send scheduled broadcasts, process pending spikes
 * HOW:  Check timers, send due broadcasts, route pending spikes
 *
 * @param bridge Bio-async bridge
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int core_bio_async_update(
    core_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Brain State Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast brain state to all subscribers
 *
 * WHAT: Send current brain state to all interested modules
 * WHY:  System-wide awareness of neural activity state
 * HOW:  Gather state from brain module, format message, broadcast
 *
 * @param bridge Bio-async bridge
 * @param active_neurons Number of active neurons
 * @param total_neurons Total neuron count
 * @param activity_level Global activity level [0, 1]
 * @return 0 on success, -1 on error
 */
int core_bio_async_broadcast_brain_state(
    core_bio_async_bridge_t* bridge,
    uint32_t active_neurons,
    uint32_t total_neurons,
    float activity_level
);

/**
 * @brief Broadcast medulla state update
 *
 * WHAT: Send medulla state to subscribers
 * WHY:  Coordinate arousal and protection across modules
 * HOW:  Package medulla state, broadcast to subscribers
 *
 * @param bridge Bio-async bridge
 * @param arousal_level Arousal level [0, 1]
 * @param protection_level Protection level [0, 1]
 * @param circadian_phase Circadian phase [0, 2*PI]
 * @return 0 on success, -1 on error
 */
int core_bio_async_broadcast_medulla_state(
    core_bio_async_bridge_t* bridge,
    float arousal_level,
    float protection_level,
    float circadian_phase
);

/**
 * @brief Broadcast topology change notification
 *
 * WHAT: Notify subscribers of topology changes
 * WHY:  Keep modules synchronized with network structure
 * HOW:  Package topology info, broadcast to subscribers
 *
 * @param bridge Bio-async bridge
 * @param connections_added New connections added
 * @param connections_removed Connections removed
 * @param neurons_added New neurons added
 * @return 0 on success, -1 on error
 */
int core_bio_async_broadcast_topology_update(
    core_bio_async_bridge_t* bridge,
    uint32_t connections_added,
    uint32_t connections_removed,
    uint32_t neurons_added
);

/* ============================================================================
 * Spike Routing API
 * ============================================================================ */

/**
 * @brief Route a single spike to post-synaptic targets
 *
 * WHAT: Route spike from pre-synaptic neuron to all targets
 * WHY:  Enable neuron-to-neuron communication via messaging
 * HOW:  Look up targets from topology, send spike messages
 *
 * @param bridge Bio-async bridge
 * @param neuron_id Pre-synaptic neuron ID
 * @param amplitude Spike amplitude
 * @param is_inhibitory True if inhibitory neuron
 * @return 0 on success, error code on failure
 */
int core_bio_async_route_spike(
    core_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    float amplitude,
    bool is_inhibitory
);

/**
 * @brief Route a batch of spikes
 *
 * WHAT: Route multiple spikes efficiently
 * WHY:  Reduce messaging overhead for high-activity periods
 * HOW:  Batch spikes by target, send consolidated messages
 *
 * @param bridge Bio-async bridge
 * @param neuron_ids Array of neuron IDs
 * @param amplitudes Array of amplitudes
 * @param spike_count Number of spikes
 * @return Number of spikes successfully routed, -1 on error
 */
int core_bio_async_route_spike_batch(
    core_bio_async_bridge_t* bridge,
    const uint32_t* neuron_ids,
    const float* amplitudes,
    uint32_t spike_count
);

/**
 * @brief Queue spike for deferred routing
 *
 * WHAT: Add spike to pending queue for later routing
 * WHY:  Allow accumulation and batch processing
 * HOW:  Add to queue, process in next update cycle
 *
 * @param bridge Bio-async bridge
 * @param neuron_id Neuron ID
 * @param amplitude Spike amplitude
 * @return 0 on success, error code on failure
 */
int core_bio_async_queue_spike(
    core_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    float amplitude
);

/**
 * @brief Process all pending spikes
 *
 * WHAT: Route all queued spikes
 * WHY:  Batch processing of accumulated spikes
 * HOW:  Drain queue, route each spike
 *
 * @param bridge Bio-async bridge
 * @return Number of spikes processed, -1 on error
 */
int core_bio_async_process_pending_spikes(core_bio_async_bridge_t* bridge);

/**
 * @brief Get pending spike count
 *
 * @param bridge Bio-async bridge
 * @return Number of pending spikes in queue
 */
uint32_t core_bio_async_get_pending_spike_count(
    const core_bio_async_bridge_t* bridge
);

/* ============================================================================
 * Health Monitoring API
 * ============================================================================ */

/**
 * @brief Perform health check on all registered modules
 *
 * WHAT: Query health status of all core modules
 * WHY:  Monitor system health and detect issues
 * HOW:  Send health check messages, collect responses
 *
 * @param bridge Bio-async bridge
 * @return 0 if all modules healthy, count of unhealthy modules
 */
int core_bio_async_health_check_all(core_bio_async_bridge_t* bridge);

/**
 * @brief Get health status of specific module
 *
 * @param bridge Bio-async bridge
 * @param type Module type
 * @param health_score Output health score [0, 1] (can be NULL)
 * @return true if module is healthy
 */
bool core_bio_async_get_module_health(
    const core_bio_async_bridge_t* bridge,
    core_module_type_t type,
    float* health_score
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bio-async bridge
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int core_bio_async_get_stats(
    const core_bio_async_bridge_t* bridge,
    core_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int core_bio_async_reset_stats(core_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name string
 */
const char* core_bio_msg_type_name(core_bio_msg_type_t msg_type);

/**
 * @brief Get module type name
 *
 * @param module_type Module type
 * @return Human-readable name string
 */
const char* core_module_type_name(core_module_type_t module_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bio-async bridge (NULL-safe)
 */
void core_bio_async_print_summary(const core_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORE_BIO_ASYNC_BRIDGE_H */
