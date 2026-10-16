/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_hh_substrate_bridge.h - Hodgkin-Huxley to Bio-Async Substrate Bridge
//=============================================================================
/**
 * @file nimcp_hh_substrate_bridge.h
 * @brief Bridge between HH biophysics and bio-async messaging substrate
 *
 * WHAT: Deep integration between Hodgkin-Huxley neuron dynamics and the
 *       bio-async messaging substrate, enabling biophysically-grounded
 *       asynchronous neural communication.
 *
 * WHY:  The bio-async substrate provides decoupled, asynchronous messaging
 *       between neural modules. HH neurons need to:
 *       - Publish spike events with precise timing metadata
 *       - Subscribe to synaptic inputs from other neurons
 *       - Report ion channel state for system-wide coordination
 *       - Receive neuromodulatory signals affecting dynamics
 *
 * HOW:  - Registers HH neurons/populations as bio-async modules
 *       - Publishes spike, voltage, and conductance messages
 *       - Handles incoming synaptic current requests
 *       - Routes temperature and modulation signals
 *       - Maintains message priority based on HH state
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * BIO-ASYNC MESSAGING MODEL:
 * --------------------------
 * The bio-async substrate mirrors biological neural communication:
 * - Action potentials: Discrete spike messages with timing
 * - Synaptic transmission: Current injection requests
 * - Neuromodulation: Broadcast channel modulation
 * - Glial signaling: Slow metabolic/temperature messages
 *
 * HH MESSAGE TYPES:
 * -----------------
 * 1. Spike Events (HH_SUBSTRATE_MSG_SPIKE):
 *    - Precise spike time (< 0.1 ms resolution)
 *    - Peak amplitude and shape features
 *    - Temperature and channel state at spike
 *    - Priority: HIGH (time-critical)
 *
 * 2. Voltage State (HH_SUBSTRATE_MSG_VOLTAGE):
 *    - Current membrane voltage
 *    - Gating variable states (m, h, n)
 *    - Near-threshold alerts
 *    - Priority: NORMAL (periodic updates)
 *
 * 3. Conductance State (HH_SUBSTRATE_MSG_CONDUCTANCE):
 *    - Ion channel conductances (g_Na, g_K, g_L)
 *    - Channel currents
 *    - Modulation factors
 *    - Priority: LOW (diagnostic)
 *
 * 4. Current Request (HH_SUBSTRATE_MSG_CURRENT_REQ):
 *    - Synaptic current injection
 *    - Duration and timing
 *    - Excitatory vs inhibitory
 *    - Priority: HIGH (affects dynamics)
 *
 * 5. Temperature Change (HH_SUBSTRATE_MSG_TEMPERATURE):
 *    - Temperature updates from thermodynamics
 *    - Q10 factor changes
 *    - Priority: NORMAL
 *
 * 6. Modulation Signal (HH_SUBSTRATE_MSG_MODULATION):
 *    - Neuromodulatory effects
 *    - Channel scaling factors
 *    - Priority: NORMAL
 *
 * ROUTING PATTERNS:
 * -----------------
 * - Unicast: Direct neuron-to-neuron synaptic transmission
 * - Multicast: Population spike broadcast
 * - Broadcast: Neuromodulatory signals to all neurons
 * - Request-Reply: Current injection with acknowledgment
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_HH_SUBSTRATE_BRIDGE_H
#define NIMCP_HH_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define HH_SUBSTRATE_MODULE_NAME        "hh_substrate_bridge"

/** Maximum subscriptions per bridge */
#define HH_SUBSTRATE_MAX_SUBSCRIPTIONS  256

/** Maximum pending inbox messages */
#define HH_SUBSTRATE_MAX_INBOX          512

/** Maximum pending outbox messages */
#define HH_SUBSTRATE_MAX_OUTBOX         256

/** Default broadcast interval (ms) */
#define HH_SUBSTRATE_BROADCAST_INTERVAL 10.0f

/** Message TTL (ms) */
#define HH_SUBSTRATE_MSG_TTL            1000.0f

/** Near-threshold alert zone (mV from threshold) */
#define HH_SUBSTRATE_THRESHOLD_ZONE     5.0f

//=============================================================================
// Message Type Enumeration
//=============================================================================

/**
 * @brief HH substrate message types
 */
typedef enum {
    HH_SUBSTRATE_MSG_SPIKE = 0,         /**< Spike event */
    HH_SUBSTRATE_MSG_VOLTAGE,           /**< Voltage state update */
    HH_SUBSTRATE_MSG_CONDUCTANCE,       /**< Conductance state */
    HH_SUBSTRATE_MSG_GATING,            /**< Detailed gating variables */
    HH_SUBSTRATE_MSG_CURRENT_REQ,       /**< Current injection request */
    HH_SUBSTRATE_MSG_CURRENT_ACK,       /**< Current injection acknowledgment */
    HH_SUBSTRATE_MSG_TEMPERATURE,       /**< Temperature change */
    HH_SUBSTRATE_MSG_MODULATION,        /**< Channel modulation signal */
    HH_SUBSTRATE_MSG_POPULATION_RATE,   /**< Population firing rate */
    HH_SUBSTRATE_MSG_THRESHOLD_ALERT,   /**< Near-threshold warning */
    HH_SUBSTRATE_MSG_COUNT
} hh_substrate_msg_type_t;

/**
 * @brief Message priority levels
 */
typedef enum {
    HH_SUBSTRATE_PRIORITY_LOW = 0,      /**< Diagnostic/optional */
    HH_SUBSTRATE_PRIORITY_NORMAL,       /**< Standard updates */
    HH_SUBSTRATE_PRIORITY_HIGH,         /**< Time-critical (spikes) */
    HH_SUBSTRATE_PRIORITY_CRITICAL      /**< Emergency (threshold alerts) */
} hh_substrate_priority_t;

/**
 * @brief Subscription mask bits
 */
#define HH_SUBSTRATE_SUB_SPIKE          (1U << HH_SUBSTRATE_MSG_SPIKE)
#define HH_SUBSTRATE_SUB_VOLTAGE        (1U << HH_SUBSTRATE_MSG_VOLTAGE)
#define HH_SUBSTRATE_SUB_CONDUCTANCE    (1U << HH_SUBSTRATE_MSG_CONDUCTANCE)
#define HH_SUBSTRATE_SUB_GATING         (1U << HH_SUBSTRATE_MSG_GATING)
#define HH_SUBSTRATE_SUB_CURRENT_REQ    (1U << HH_SUBSTRATE_MSG_CURRENT_REQ)
#define HH_SUBSTRATE_SUB_TEMPERATURE    (1U << HH_SUBSTRATE_MSG_TEMPERATURE)
#define HH_SUBSTRATE_SUB_MODULATION     (1U << HH_SUBSTRATE_MSG_MODULATION)
#define HH_SUBSTRATE_SUB_POPULATION     (1U << HH_SUBSTRATE_MSG_POPULATION_RATE)
#define HH_SUBSTRATE_SUB_THRESHOLD      (1U << HH_SUBSTRATE_MSG_THRESHOLD_ALERT)
#define HH_SUBSTRATE_SUB_ALL            (0xFFFFFFFFU)

//=============================================================================
// Message Payload Structures
//=============================================================================

/**
 * @brief Spike message payload
 */
typedef struct {
    uint32_t neuron_id;               /**< Source neuron ID */
    uint32_t population_id;           /**< Population ID (0 if single) */
    float spike_time_ms;              /**< Precise spike time */
    float peak_voltage_mv;            /**< Peak membrane voltage */
    float spike_width_ms;             /**< Spike width at half-max */
    float isi_ms;                     /**< Inter-spike interval */
    float instantaneous_rate_hz;      /**< Instantaneous firing rate */
    float temperature;                /**< Temperature at spike */
    float phi_factor;                 /**< Q10 factor at spike */
    uint32_t spike_count;             /**< Total spike count */
    uint64_t timestamp_us;            /**< Message timestamp */
} hh_substrate_spike_msg_t;

/**
 * @brief Voltage state message payload
 */
typedef struct {
    uint32_t neuron_id;               /**< Neuron ID */
    float membrane_voltage;           /**< Current voltage (mV) */
    float previous_voltage;           /**< Previous voltage (mV) */
    float dv_dt;                      /**< Voltage derivative (mV/ms) */
    float m_gate;                     /**< Na+ activation [0,1] */
    float h_gate;                     /**< Na+ inactivation [0,1] */
    float n_gate;                     /**< K+ activation [0,1] */
    bool approaching_threshold;       /**< True if depolarizing toward threshold */
    uint64_t timestamp_us;            /**< Message timestamp */
} hh_substrate_voltage_msg_t;

/**
 * @brief Conductance state message payload
 */
typedef struct {
    uint32_t neuron_id;               /**< Neuron ID */
    float g_na;                       /**< Na+ conductance (mS/cm^2) */
    float g_k;                        /**< K+ conductance (mS/cm^2) */
    float g_l;                        /**< Leak conductance (mS/cm^2) */
    float g_ca;                       /**< Ca2+ conductance (if enabled) */
    float i_na;                       /**< Na+ current (uA/cm^2) */
    float i_k;                        /**< K+ current (uA/cm^2) */
    float i_l;                        /**< Leak current (uA/cm^2) */
    float i_total;                    /**< Total membrane current */
    uint64_t timestamp_us;            /**< Message timestamp */
} hh_substrate_conductance_msg_t;

/**
 * @brief Current injection request payload
 */
typedef struct {
    uint32_t requester_id;            /**< Requesting module ID */
    uint32_t target_neuron;           /**< Target neuron ID */
    uint32_t target_population;       /**< Target population (0 = specific) */
    float current_amount;             /**< Current to inject (uA/cm^2) */
    float duration_ms;                /**< Injection duration */
    float start_time_ms;              /**< Injection start time */
    bool is_excitatory;               /**< Excitatory (true) or inhibitory */
    bool requires_ack;                /**< Request acknowledgment */
    uint32_t request_id;              /**< Request ID for tracking */
    uint64_t timestamp_us;            /**< Request timestamp */
} hh_substrate_current_req_msg_t;

/**
 * @brief Current injection acknowledgment payload
 */
typedef struct {
    uint32_t target_neuron;           /**< Neuron that received current */
    uint32_t request_id;              /**< Original request ID */
    float actual_current;             /**< Actual current applied */
    float actual_duration;            /**< Actual duration */
    bool success;                     /**< Injection success flag */
    uint64_t timestamp_us;            /**< Ack timestamp */
} hh_substrate_current_ack_msg_t;

/**
 * @brief Temperature change message payload
 */
typedef struct {
    uint32_t source_module;           /**< Source of temperature change */
    float new_temperature;            /**< New temperature (C) */
    float old_temperature;            /**< Previous temperature (C) */
    float phi_factor;                 /**< Computed Q10 factor */
    float transition_duration_ms;     /**< Duration of temperature change */
    uint64_t timestamp_us;            /**< Message timestamp */
} hh_substrate_temperature_msg_t;

/**
 * @brief Channel modulation message payload
 */
typedef struct {
    uint32_t source_module;           /**< Source of modulation */
    uint32_t target_neuron;           /**< Target neuron (0 = all) */
    uint32_t target_population;       /**< Target population (0 = all) */
    float g_na_scale;                 /**< Na+ conductance scaling [0.5, 2.0] */
    float g_k_scale;                  /**< K+ conductance scaling */
    float g_ca_scale;                 /**< Ca2+ conductance scaling */
    float threshold_shift;            /**< Threshold adjustment (mV) */
    float duration_ms;                /**< Modulation duration (0 = permanent) */
    uint64_t timestamp_us;            /**< Message timestamp */
} hh_substrate_modulation_msg_t;

/**
 * @brief Population rate message payload
 */
typedef struct {
    uint32_t population_id;           /**< Population ID */
    float mean_rate_hz;               /**< Mean firing rate */
    float std_rate_hz;                /**< Rate standard deviation */
    float synchrony;                  /**< Population synchrony [0,1] */
    uint32_t active_neurons;          /**< Number of active neurons */
    uint32_t total_neurons;           /**< Total population size */
    float measurement_window_ms;      /**< Window for measurement */
    uint64_t timestamp_us;            /**< Message timestamp */
} hh_substrate_population_msg_t;

/**
 * @brief Threshold alert message payload
 */
typedef struct {
    uint32_t neuron_id;               /**< Neuron near threshold */
    float current_voltage;            /**< Current voltage (mV) */
    float threshold_voltage;          /**< Threshold voltage (mV) */
    float distance_mv;                /**< Distance to threshold */
    float voltage_trend;              /**< dV/dt (mV/ms) */
    float estimated_time_to_spike_ms; /**< Estimated time to spike */
    uint64_t timestamp_us;            /**< Alert timestamp */
} hh_substrate_threshold_alert_msg_t;

//=============================================================================
// Subscription Structure
//=============================================================================

/**
 * @brief Module subscription entry
 */
typedef struct {
    uint32_t module_id;               /**< Subscribed module ID */
    uint32_t msg_type_mask;           /**< Bitmask of subscribed types */
    bool active;                      /**< Subscription active flag */
    uint64_t subscribe_time;          /**< When subscribed */
    uint64_t messages_delivered;      /**< Messages sent to subscriber */
} hh_substrate_subscription_t;

//=============================================================================
// Bridge Configuration
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Broadcasting options */
    bool enable_auto_broadcast;       /**< Auto-broadcast voltage state */
    float broadcast_interval_ms;      /**< Voltage broadcast interval */
    bool broadcast_spikes;            /**< Broadcast on every spike */
    bool broadcast_conductance;       /**< Include conductance in broadcasts */
    bool broadcast_threshold_alerts;  /**< Send near-threshold warnings */

    /* Message handling */
    uint32_t max_inbox_messages;      /**< Maximum inbox size */
    uint32_t max_outbox_messages;     /**< Maximum outbox size */
    float message_ttl_ms;             /**< Message time-to-live */
    uint32_t max_process_per_update;  /**< Max inbox messages per update */

    /* Priority settings */
    hh_substrate_priority_t spike_priority;     /**< Priority for spikes */
    hh_substrate_priority_t voltage_priority;   /**< Priority for voltage */
    hh_substrate_priority_t current_priority;   /**< Priority for current req */

    /* Threshold alerts */
    float threshold_alert_zone_mv;    /**< Distance for threshold alerts */
    bool enable_spike_prediction;     /**< Predict upcoming spikes */

    /* Subscription limits */
    uint32_t max_subscriptions;       /**< Maximum module subscriptions */

    /* Logging */
    bool enable_logging;              /**< Enable message logging */
} hh_substrate_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;           /**< Total messages sent */
    uint64_t messages_received;       /**< Total messages received */
    uint64_t messages_dropped;        /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;         /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t spike_messages;          /**< Spike messages sent */
    uint64_t voltage_updates;         /**< Voltage updates sent */
    uint64_t conductance_updates;     /**< Conductance updates sent */
    uint64_t current_requests;        /**< Current requests processed */
    uint64_t temperature_updates;     /**< Temperature updates processed */
    uint64_t modulation_signals;      /**< Modulation signals processed */
    uint64_t threshold_alerts;        /**< Threshold alerts sent */

    /* Subscription stats */
    uint32_t active_subscriptions;    /**< Currently active subscriptions */
    uint32_t peak_subscriptions;      /**< Peak subscription count */

    /* Timing stats */
    float avg_message_latency_us;     /**< Average message latency */
    float max_message_latency_us;     /**< Maximum message latency */
    float last_update_ms;             /**< Last update timestamp */

    /* Error counts */
    uint64_t delivery_failures;       /**< Failed deliveries */
    uint64_t routing_errors;          /**< Routing errors */
} hh_substrate_stats_t;

/** Opaque bridge handle */
typedef struct hh_substrate_bridge_struct hh_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy bridge creation with standard messaging
 * HOW:  Set auto-broadcast, priorities, limits
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_default_config(hh_substrate_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create HH-substrate bridge
 *
 * WHAT: Initialize bridge for bio-async messaging
 * WHY:  Enable asynchronous HH communication
 * HOW:  Allocate queues, initialize subscription registry
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT hh_substrate_bridge_t* hh_substrate_bridge_create(
    const hh_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void hh_substrate_bridge_destroy(hh_substrate_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_bridge_reset(hh_substrate_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect bridge to bio-router
 *
 * WHAT: Register bridge with bio-async router
 * WHY:  Enable message routing through substrate
 * HOW:  Register as module, set up handlers
 *
 * @param bridge Bridge handle
 * @param router Bio-router handle (opaque)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_connect_router(
    hh_substrate_bridge_t* bridge,
    void* router
);

/**
 * @brief Disconnect from bio-router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_disconnect_router(hh_substrate_bridge_t* bridge);

/**
 * @brief Check if connected to router
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
NIMCP_EXPORT bool hh_substrate_is_connected(const hh_substrate_bridge_t* bridge);

//=============================================================================
// Publishing API (HH to Substrate)
//=============================================================================

/**
 * @brief Publish spike event
 *
 * WHAT: Broadcast spike event to subscribers
 * WHY:  Notify other modules of HH spike
 * HOW:  Create message, route to subscribers
 *
 * @param bridge Bridge handle
 * @param spike Spike message payload
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_publish_spike(
    hh_substrate_bridge_t* bridge,
    const hh_substrate_spike_msg_t* spike
);

/**
 * @brief Publish voltage state
 *
 * WHAT: Broadcast voltage state update
 * WHY:  Share continuous voltage for monitoring
 * HOW:  Create message, route to subscribers
 *
 * @param bridge Bridge handle
 * @param voltage Voltage message payload
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_publish_voltage(
    hh_substrate_bridge_t* bridge,
    const hh_substrate_voltage_msg_t* voltage
);

/**
 * @brief Publish conductance state
 *
 * WHAT: Broadcast ion channel conductances
 * WHY:  Share channel state for diagnostics
 * HOW:  Create message, route to subscribers
 *
 * @param bridge Bridge handle
 * @param conductance Conductance message payload
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_publish_conductance(
    hh_substrate_bridge_t* bridge,
    const hh_substrate_conductance_msg_t* conductance
);

/**
 * @brief Publish population rate
 *
 * WHAT: Broadcast population firing statistics
 * WHY:  Share population-level dynamics
 * HOW:  Create message, route to subscribers
 *
 * @param bridge Bridge handle
 * @param population Population rate message payload
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_publish_population_rate(
    hh_substrate_bridge_t* bridge,
    const hh_substrate_population_msg_t* population
);

/**
 * @brief Send threshold alert
 *
 * WHAT: Warn subscribers of imminent spike
 * WHY:  Enable predictive processing
 * HOW:  Create alert message, high priority route
 *
 * @param bridge Bridge handle
 * @param alert Threshold alert payload
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_send_threshold_alert(
    hh_substrate_bridge_t* bridge,
    const hh_substrate_threshold_alert_msg_t* alert
);

//=============================================================================
// Receiving API (Substrate to HH)
//=============================================================================

/**
 * @brief Process incoming messages
 *
 * WHAT: Process messages from inbox
 * WHY:  Handle current requests, modulation, temperature
 * HOW:  Dequeue messages, invoke handlers
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, -1 on error
 */
NIMCP_EXPORT int hh_substrate_process_inbox(
    hh_substrate_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Get pending current request
 *
 * WHAT: Retrieve next current injection request
 * WHY:  Apply synaptic inputs to HH neurons
 * HOW:  Dequeue current request from inbox
 *
 * @param bridge Bridge handle
 * @param request_out Output current request
 * @return 1 if request available, 0 if none, -1 on error
 */
NIMCP_EXPORT int hh_substrate_get_current_request(
    hh_substrate_bridge_t* bridge,
    hh_substrate_current_req_msg_t* request_out
);

/**
 * @brief Acknowledge current request
 *
 * WHAT: Send acknowledgment for current injection
 * WHY:  Confirm current was applied
 * HOW:  Create ack message, route to requester
 *
 * @param bridge Bridge handle
 * @param ack Acknowledgment payload
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_ack_current_request(
    hh_substrate_bridge_t* bridge,
    const hh_substrate_current_ack_msg_t* ack
);

/**
 * @brief Get pending temperature update
 *
 * WHAT: Retrieve temperature change message
 * WHY:  Apply temperature changes to HH neurons
 * HOW:  Dequeue temperature message from inbox
 *
 * @param bridge Bridge handle
 * @param temp_out Output temperature message
 * @return 1 if update available, 0 if none, -1 on error
 */
NIMCP_EXPORT int hh_substrate_get_temperature_update(
    hh_substrate_bridge_t* bridge,
    hh_substrate_temperature_msg_t* temp_out
);

/**
 * @brief Get pending modulation signal
 *
 * WHAT: Retrieve channel modulation message
 * WHY:  Apply neuromodulation to HH channels
 * HOW:  Dequeue modulation message from inbox
 *
 * @param bridge Bridge handle
 * @param mod_out Output modulation message
 * @return 1 if signal available, 0 if none, -1 on error
 */
NIMCP_EXPORT int hh_substrate_get_modulation(
    hh_substrate_bridge_t* bridge,
    hh_substrate_modulation_msg_t* mod_out
);

//=============================================================================
// Subscription API
//=============================================================================

/**
 * @brief Subscribe module to HH messages
 *
 * WHAT: Register module for message delivery
 * WHY:  Enable modules to receive HH updates
 * HOW:  Add to subscription registry
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (HH_SUBSTRATE_SUB_*)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_subscribe(
    hh_substrate_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_unsubscribe(
    hh_substrate_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update subscription types
 *
 * @param bridge Bridge handle
 * @param module_id Module to update
 * @param msg_types New message type bitmask
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_update_subscription(
    hh_substrate_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscriber count for message type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type to query
 * @return Number of subscribers
 */
NIMCP_EXPORT uint32_t hh_substrate_get_subscriber_count(
    const hh_substrate_bridge_t* bridge,
    hh_substrate_msg_type_t msg_type
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic bridge housekeeping
 * WHY:  Process messages, auto-broadcasts, TTL expiry
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_bridge_update(
    hh_substrate_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_get_stats(
    const hh_substrate_bridge_t* bridge,
    hh_substrate_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_substrate_reset_stats(hh_substrate_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Static string name
 */
NIMCP_EXPORT const char* hh_substrate_msg_type_name(hh_substrate_msg_type_t msg_type);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
NIMCP_EXPORT void hh_substrate_print_summary(const hh_substrate_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_SUBSTRATE_BRIDGE_H */