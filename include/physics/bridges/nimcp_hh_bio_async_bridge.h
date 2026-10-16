/**
 * @file nimcp_hh_bio_async_bridge.h
 * @brief Hodgkin-Huxley Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Central bio-async integration for Hodgkin-Huxley neurons that provides
 *       comprehensive message routing for biophysical neuron state via bio-router.
 *
 * WHY: The HH model needs to communicate:
 *      - Membrane voltage and spike events to downstream processors
 *      - Ion channel conductance for neuromodulation
 *      - Population-level activity for network coordination
 *      - Temperature sensitivity for thermal regulation
 *
 * HOW: Registers HH as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming modulation.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * HH OUTPUT SIGNALS:
 * ------------------
 * 1. Spike events:
 *    - Action potential threshold crossings
 *    - Timing for spike-timing dependent plasticity
 *    - Mapped to: HH_BIO_MSG_SPIKE_EVENT
 *
 * 2. Voltage state:
 *    - Continuous membrane potential
 *    - Subthreshold oscillations
 *    - Mapped to: HH_BIO_MSG_VOLTAGE_STATE
 *
 * 3. Ion channel dynamics:
 *    - Conductance states for pharmacological modeling
 *    - Channel modulation effects
 *    - Mapped to: HH_BIO_MSG_CONDUCTANCE, HH_BIO_MSG_GATING_STATE
 *
 * HH INPUT SIGNALS:
 * -----------------
 * 1. Current injection:
 *    - Synaptic currents from upstream neurons
 *    - External stimulation
 *
 * 2. Temperature modulation:
 *    - From thermodynamics module
 *    - Affects Q10 rate scaling
 *
 * 3. Channel modulation:
 *    - Neuromodulatory effects
 *    - Pharmacological interventions
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HH_BIO_ASYNC_BRIDGE_H
#define NIMCP_HH_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define HH_BIO_MAX_SUBSCRIPTIONS        64

/** Maximum pending messages in inbox */
#define HH_BIO_MAX_INBOX_SIZE           256

/** Maximum pending messages in outbox */
#define HH_BIO_MAX_OUTBOX_SIZE          128

/** Default broadcast interval for voltage state (ms) */
#define HH_BIO_DEFAULT_BROADCAST_INTERVAL_MS  25

/** Message expiry time (ms) */
#define HH_BIO_MESSAGE_TTL_MS           5000

/** Spike detection voltage threshold (mV) */
#define HH_BIO_SPIKE_THRESHOLD          0.0f

/** Near-threshold alert zone (mV from threshold) */
#define HH_BIO_THRESHOLD_ALERT_ZONE     5.0f

/* ============================================================================
 * Message Types (0x1300-0x131F range in nimcp_bio_messages.h)
 * ============================================================================ */

/**
 * @brief HH bio-async message types
 *
 * WHAT: Message type enumeration for HH bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific HH output pathway
 */
typedef enum {
    HH_BIO_MSG_VOLTAGE_STATE = 0,       /**< Membrane voltage broadcast */
    HH_BIO_MSG_SPIKE_EVENT,             /**< Action potential detected */
    HH_BIO_MSG_CONDUCTANCE,             /**< Ion channel conductance */
    HH_BIO_MSG_GATING_STATE,            /**< m, h, n gating variables */
    HH_BIO_MSG_CURRENT_REQUEST,         /**< Request current injection */
    HH_BIO_MSG_TEMPERATURE_CHANGE,      /**< Temperature modulation */
    HH_BIO_MSG_POPULATION_RATE,         /**< Population firing rate */
    HH_BIO_MSG_THRESHOLD_ALERT,         /**< Near-threshold warning */
    HH_BIO_MSG_COUNT
} hh_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define HH_BIO_SUB_VOLTAGE_STATE        (1U << HH_BIO_MSG_VOLTAGE_STATE)
#define HH_BIO_SUB_SPIKE_EVENT          (1U << HH_BIO_MSG_SPIKE_EVENT)
#define HH_BIO_SUB_CONDUCTANCE          (1U << HH_BIO_MSG_CONDUCTANCE)
#define HH_BIO_SUB_GATING_STATE         (1U << HH_BIO_MSG_GATING_STATE)
#define HH_BIO_SUB_CURRENT_REQUEST      (1U << HH_BIO_MSG_CURRENT_REQUEST)
#define HH_BIO_SUB_TEMPERATURE_CHANGE   (1U << HH_BIO_MSG_TEMPERATURE_CHANGE)
#define HH_BIO_SUB_POPULATION_RATE      (1U << HH_BIO_MSG_POPULATION_RATE)
#define HH_BIO_SUB_THRESHOLD_ALERT      (1U << HH_BIO_MSG_THRESHOLD_ALERT)
#define HH_BIO_SUB_ALL                  (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Voltage state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Voltage state */
    float membrane_voltage;             /**< Membrane voltage (mV) */
    float previous_voltage;             /**< Previous voltage for delta */
    float voltage_derivative;           /**< dV/dt (mV/ms) */

    /* Gating variables (compact) */
    float m;                            /**< Na activation [0, 1] */
    float h;                            /**< Na inactivation [0, 1] */
    float n;                            /**< K activation [0, 1] */

    /* Neuron identification */
    uint32_t neuron_id;                 /**< Neuron identifier */
    uint32_t population_id;             /**< Population (0 if single) */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hh_bio_voltage_msg_t;

/**
 * @brief Spike event message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Spike data */
    float spike_amplitude;              /**< Peak voltage (mV) */
    float spike_width;                  /**< Spike width at half-max (ms) */
    float interspike_interval;          /**< Time since last spike (ms) */

    /* Neuron state at spike */
    float pre_spike_voltage;            /**< Voltage before spike */
    float instantaneous_rate;           /**< Current firing rate (Hz) */

    /* Identification */
    uint32_t neuron_id;                 /**< Neuron identifier */
    uint32_t spike_number;              /**< Total spike count */

    uint64_t spike_time_us;             /**< Precise spike timestamp */
} hh_bio_spike_msg_t;

/**
 * @brief Ion channel conductance message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Conductances (mS/cm^2) */
    float g_Na;                         /**< Sodium conductance */
    float g_K;                          /**< Potassium conductance */
    float g_L;                          /**< Leak conductance */
    float g_Ca;                         /**< Calcium conductance (if enabled) */

    /* Currents (uA/cm^2) */
    float I_Na;                         /**< Sodium current */
    float I_K;                          /**< Potassium current */
    float I_L;                          /**< Leak current */
    float I_total;                      /**< Total membrane current */

    /* Identification */
    uint32_t neuron_id;                 /**< Neuron identifier */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hh_bio_conductance_msg_t;

/**
 * @brief Gating state message payload (detailed)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Activation gates */
    float m_value;                      /**< Na activation value */
    float m_inf;                        /**< Na activation steady-state */
    float m_tau;                        /**< Na activation time constant */

    /* Inactivation gates */
    float h_value;                      /**< Na inactivation value */
    float h_inf;                        /**< Na inactivation steady-state */
    float h_tau;                        /**< Na inactivation time constant */

    /* Potassium gate */
    float n_value;                      /**< K activation value */
    float n_inf;                        /**< K activation steady-state */
    float n_tau;                        /**< K activation time constant */

    /* Identification */
    uint32_t neuron_id;                 /**< Neuron identifier */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hh_bio_gating_msg_t;

/**
 * @brief Current injection request payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float current_amount;               /**< Current to inject (uA/cm^2) */
    float duration_ms;                  /**< Injection duration (ms) */
    bool is_excitatory;                 /**< Excitatory vs inhibitory */

    uint32_t requester_module;          /**< Who is requesting */
    uint32_t target_neuron;             /**< Target neuron ID */
    uint32_t target_population;         /**< Target population (0 = all) */

    uint64_t timestamp_us;              /**< Request timestamp */
} hh_bio_current_request_msg_t;

/**
 * @brief Temperature change message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float new_temperature;              /**< New temperature (Celsius) */
    float old_temperature;              /**< Previous temperature */
    float phi_factor;                   /**< Computed Q10 factor */

    uint32_t source_module;             /**< Source of temperature change */

    uint64_t timestamp_us;              /**< Change timestamp */
} hh_bio_temperature_msg_t;

/**
 * @brief Population firing rate message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float mean_rate;                    /**< Mean firing rate (Hz) */
    float std_rate;                     /**< Standard deviation */
    float synchrony;                    /**< Population synchrony [0, 1] */

    uint32_t active_neurons;            /**< Number currently active */
    uint32_t total_neurons;             /**< Total population size */
    uint32_t spikes_this_window;        /**< Spikes in measurement window */

    float window_ms;                    /**< Measurement window (ms) */
    uint32_t population_id;             /**< Population identifier */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hh_bio_population_rate_msg_t;

/**
 * @brief Threshold alert message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float current_voltage;              /**< Current membrane voltage */
    float threshold_voltage;            /**< Threshold voltage */
    float distance_to_threshold;        /**< mV from threshold */
    float voltage_trend;                /**< dV/dt direction */

    bool approaching_threshold;         /**< True if depolarizing */
    uint32_t neuron_id;                 /**< Neuron identifier */

    uint64_t timestamp_us;              /**< Alert timestamp */
} hh_bio_threshold_alert_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;          /**< Subscribed module ID */
    uint32_t msg_type_mask;             /**< Bitmask of subscribed types */
    bool active;                        /**< Subscription active */
    uint64_t subscription_time;         /**< When subscribed */
    uint64_t messages_sent;             /**< Messages sent to this sub */
} hh_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief HH bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t broadcast_interval_ms;          /**< Voltage state broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast voltage state */
    bool enable_spike_broadcast;             /**< Broadcast on every spike */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float spike_threshold_mv;                /**< Spike detection threshold */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t spike_channel;   /**< Channel for spike events */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_conductance_broadcast;       /**< Broadcast conductance state */
    bool enable_gating_broadcast;            /**< Broadcast detailed gating */
    bool enable_threshold_alerts;            /**< Enable near-threshold alerts */
    bool enable_population_stats;            /**< Enable population statistics */
    bool enable_logging;                     /**< Enable message logging */
} hh_bio_async_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                  /**< Total messages sent */
    uint64_t messages_received;              /**< Total messages received */
    uint64_t messages_dropped;               /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;                /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t voltage_broadcasts;             /**< Voltage state broadcasts */
    uint64_t spike_broadcasts;               /**< Spike event notifications */
    uint64_t conductance_broadcasts;         /**< Conductance broadcasts */
    uint64_t population_broadcasts;          /**< Population rate broadcasts */
    uint64_t threshold_alerts_sent;          /**< Threshold alerts sent */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active subs */
    uint32_t peak_subscriptions;             /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;            /**< Average message latency */
    float max_message_latency_us;            /**< Peak message latency */

    /* Error counts */
    uint64_t handler_errors;                 /**< Message handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} hh_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief HH bio-async bridge handle
 */
typedef struct hh_bio_async_bridge_struct hh_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int hh_bio_async_default_config(hh_bio_async_config_t* config);

/**
 * @brief Create HH bio-async bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
hh_bio_async_bridge_t* hh_bio_async_bridge_create(
    const hh_bio_async_config_t* config
);

/**
 * @brief Destroy HH bio-async bridge
 *
 * @param bridge Bridge to destroy
 */
void hh_bio_async_bridge_destroy(hh_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to HH neuron/population and router
 *
 * @param bridge Bridge handle
 * @param neuron Single neuron to connect (can be NULL if population)
 * @param population Population to connect (can be NULL if single neuron)
 * @param router Bio-router for message dispatch
 * @return 0 on success, -1 on error
 */
int hh_bio_async_connect(
    hh_bio_async_bridge_t* bridge,
    nimcp_hh_neuron_t* neuron,
    nimcp_hh_population_t* population,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int hh_bio_async_disconnect(hh_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool hh_bio_async_is_connected(const hh_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int hh_bio_async_process_inbox(
    hh_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
int hh_bio_async_update(
    hh_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast voltage state for a single neuron
 *
 * @param bridge Bridge handle
 * @param neuron Neuron to broadcast (NULL for connected neuron)
 * @return 0 on success, -1 on error
 */
int hh_bio_async_broadcast_voltage(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
);

/**
 * @brief Broadcast spike event
 *
 * @param bridge Bridge handle
 * @param neuron Neuron that spiked
 * @param spike_time_us Precise spike timestamp
 * @return 0 on success, -1 on error
 */
int hh_bio_async_broadcast_spike(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron,
    uint64_t spike_time_us
);

/**
 * @brief Broadcast conductance state
 *
 * @param bridge Bridge handle
 * @param neuron Neuron to broadcast
 * @return 0 on success, -1 on error
 */
int hh_bio_async_broadcast_conductance(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
);

/**
 * @brief Broadcast detailed gating state
 *
 * @param bridge Bridge handle
 * @param neuron Neuron to broadcast
 * @return 0 on success, -1 on error
 */
int hh_bio_async_broadcast_gating(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
);

/**
 * @brief Broadcast population firing rate
 *
 * @param bridge Bridge handle
 * @param population Population to broadcast
 * @return 0 on success, -1 on error
 */
int hh_bio_async_broadcast_population_rate(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_population_t* population
);

/**
 * @brief Send threshold alert
 *
 * @param bridge Bridge handle
 * @param neuron Neuron near threshold
 * @return 0 on success, -1 on error
 */
int hh_bio_async_send_threshold_alert(
    hh_bio_async_bridge_t* bridge,
    const nimcp_hh_neuron_t* neuron
);

/**
 * @brief Broadcast temperature change
 *
 * @param bridge Bridge handle
 * @param old_temp Previous temperature
 * @param new_temp New temperature
 * @return 0 on success, -1 on error
 */
int hh_bio_async_broadcast_temperature(
    hh_bio_async_bridge_t* bridge,
    float old_temp,
    float new_temp
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to HH messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (use HH_BIO_SUB_* macros)
 * @return 0 on success, -1 on error
 */
int hh_bio_async_subscribe_module(
    hh_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from HH messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int hh_bio_async_unsubscribe_module(
    hh_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 *
 * @param bridge Bridge handle
 * @param module_id Module to update
 * @param msg_types New bitmask of message types
 * @return 0 on success, -1 on error
 */
int hh_bio_async_update_subscription(
    hh_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type to query
 * @return Number of subscribers
 */
uint32_t hh_bio_async_get_subscriber_count(
    const hh_bio_async_bridge_t* bridge,
    hh_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hh_bio_async_get_stats(
    const hh_bio_async_bridge_t* bridge,
    hh_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hh_bio_async_reset_stats(hh_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Static string name
 */
const char* hh_bio_msg_type_name(hh_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void hh_bio_async_print_summary(const hh_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_BIO_ASYNC_BRIDGE_H */
