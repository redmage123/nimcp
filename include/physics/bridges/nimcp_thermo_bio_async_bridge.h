/**
 * @file nimcp_thermo_bio_async_bridge.h
 * @brief Thermodynamics Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Central bio-async integration for non-equilibrium thermodynamics that
 *       provides comprehensive message routing for energy/entropy state.
 *
 * WHY: The thermodynamics module needs to communicate:
 *      - ATP levels and metabolic state for energy-aware processing
 *      - Temperature changes affecting rate constants
 *      - Entropy production for free energy tracking
 *      - Energy budget for resource allocation
 *
 * HOW: Registers thermodynamics as a bio-router module, maintains subscriptions,
 *      provides typed message broadcast APIs, and processes incoming requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * THERMODYNAMIC OUTPUT SIGNALS:
 * -----------------------------
 * 1. ATP/Energy state:
 *    - Current metabolic resources available
 *    - Energy budget constraints
 *    - Mapped to: THERMO_BIO_MSG_ATP_LEVEL, THERMO_BIO_MSG_ENERGY_BUDGET
 *
 * 2. Critical alerts:
 *    - ATP depletion warnings
 *    - Metabolic stress signals
 *    - Mapped to: THERMO_BIO_MSG_ATP_CRITICAL
 *
 * 3. Entropy/Efficiency:
 *    - Entropy production rate
 *    - Thermodynamic efficiency metrics
 *    - Mapped to: THERMO_BIO_MSG_ENTROPY, THERMO_BIO_MSG_EFFICIENCY
 *
 * THERMODYNAMIC INPUT SIGNALS:
 * ----------------------------
 * 1. Energy consumption reports from active modules
 * 2. Temperature changes from environment/hypothalamus
 * 3. Metabolic demand signals
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

#ifndef NIMCP_THERMO_BIO_ASYNC_BRIDGE_H
#define NIMCP_THERMO_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define THERMO_BIO_MAX_SUBSCRIPTIONS        64

/** Maximum pending messages in inbox */
#define THERMO_BIO_MAX_INBOX_SIZE           128

/** Maximum pending messages in outbox */
#define THERMO_BIO_MAX_OUTBOX_SIZE          64

/** Default broadcast interval for state (ms) */
#define THERMO_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100

/** Message expiry time (ms) */
#define THERMO_BIO_MESSAGE_TTL_MS           10000

/** ATP critical threshold (fraction of pool) */
#define THERMO_BIO_ATP_CRITICAL_THRESHOLD   0.1f

/** ATP warning threshold (fraction of pool) */
#define THERMO_BIO_ATP_WARNING_THRESHOLD    0.3f

/* ============================================================================
 * Message Types (0x1320-0x133F range)
 * ============================================================================ */

/**
 * @brief Thermodynamics bio-async message types
 *
 * WHAT: Message type enumeration for thermodynamics bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific thermodynamic output
 */
typedef enum {
    THERMO_BIO_MSG_TEMPERATURE = 0,     /**< Temperature state */
    THERMO_BIO_MSG_HEAT_FLOW,           /**< Heat transfer rate */
    THERMO_BIO_MSG_ENTROPY,             /**< Entropy production */
    THERMO_BIO_MSG_ATP_LEVEL,           /**< ATP pool status */
    THERMO_BIO_MSG_ATP_CRITICAL,        /**< ATP critically low */
    THERMO_BIO_MSG_ENERGY_BUDGET,       /**< Energy consumption report */
    THERMO_BIO_MSG_LANDAUER_COST,       /**< Computation cost */
    THERMO_BIO_MSG_EFFICIENCY,          /**< Thermodynamic efficiency */
    THERMO_BIO_MSG_COUNT
} thermo_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define THERMO_BIO_SUB_TEMPERATURE      (1U << THERMO_BIO_MSG_TEMPERATURE)
#define THERMO_BIO_SUB_HEAT_FLOW        (1U << THERMO_BIO_MSG_HEAT_FLOW)
#define THERMO_BIO_SUB_ENTROPY          (1U << THERMO_BIO_MSG_ENTROPY)
#define THERMO_BIO_SUB_ATP_LEVEL        (1U << THERMO_BIO_MSG_ATP_LEVEL)
#define THERMO_BIO_SUB_ATP_CRITICAL     (1U << THERMO_BIO_MSG_ATP_CRITICAL)
#define THERMO_BIO_SUB_ENERGY_BUDGET    (1U << THERMO_BIO_MSG_ENERGY_BUDGET)
#define THERMO_BIO_SUB_LANDAUER_COST    (1U << THERMO_BIO_MSG_LANDAUER_COST)
#define THERMO_BIO_SUB_EFFICIENCY       (1U << THERMO_BIO_MSG_EFFICIENCY)
#define THERMO_BIO_SUB_ALL              (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Temperature state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    double temperature_k;               /**< Current temperature (Kelvin) */
    double temperature_c;               /**< Current temperature (Celsius) */
    double temperature_change_rate;     /**< Rate of change (K/s) */

    double landauer_limit;              /**< kT*ln(2) at current temp (J/bit) */

    uint32_t module_id;                 /**< Source module */
    uint64_t timestamp_us;              /**< Measurement timestamp */
} thermo_bio_temperature_msg_t;

/**
 * @brief Heat flow message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    double heat_dissipation_rate;       /**< Current heat output (W) */
    double heat_absorbed;               /**< Heat absorbed from work (W) */
    double net_heat_flow;               /**< Net thermal balance (W) */

    double external_temp_k;             /**< External temperature for flow */

    uint32_t module_id;                 /**< Source module */
    uint64_t timestamp_us;              /**< Measurement timestamp */
} thermo_bio_heat_flow_msg_t;

/**
 * @brief Entropy production message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    double entropy_production_rate;     /**< dS/dt (W/K) */
    double total_entropy_produced;      /**< Cumulative entropy (J/K) */
    double free_energy_dissipated;      /**< Free energy lost (J) */

    double irreversible_component;      /**< sigma_irr (W/K) */
    double heat_component;              /**< Q/T component (W/K) */

    uint32_t module_id;                 /**< Source module */
    uint64_t timestamp_us;              /**< Measurement timestamp */
} thermo_bio_entropy_msg_t;

/**
 * @brief ATP level message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    double atp_available;               /**< Current ATP pool (moles) */
    double atp_consumption_rate;        /**< Consumption rate (moles/s) */
    double atp_regeneration_rate;       /**< Regeneration rate (moles/s) */
    double atp_fraction;                /**< Fraction of initial pool [0, 1] */

    double time_to_depletion_s;         /**< Estimated time until empty (s) */
    bool is_critical;                   /**< Below critical threshold */
    bool is_warning;                    /**< Below warning threshold */

    uint32_t module_id;                 /**< Source module */
    uint64_t timestamp_us;              /**< Measurement timestamp */
} thermo_bio_atp_level_msg_t;

/**
 * @brief ATP critical alert message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    double atp_remaining;               /**< Remaining ATP (moles) */
    double atp_fraction;                /**< Fraction remaining [0, 1] */
    double consumption_rate;            /**< Current drain (moles/s) */
    double time_to_depletion_s;         /**< Estimated time left (s) */

    uint32_t severity;                  /**< 0=warning, 1=critical, 2=emergency */
    uint32_t module_id;                 /**< Affected module */

    uint64_t timestamp_us;              /**< Alert timestamp */
} thermo_bio_atp_critical_msg_t;

/**
 * @brief Energy budget message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    double total_energy_consumed;       /**< Total energy used (J) */
    double power_consumption;           /**< Current power (W) */

    /* Budget breakdown */
    double ion_pumping_energy;          /**< Na/K-ATPase (J) */
    double synaptic_energy;             /**< Vesicle cycling (J) */
    double computation_energy;          /**< Information processing (J) */
    double housekeeping_energy;         /**< Maintenance (J) */
    double waste_heat;                  /**< Dissipated heat (J) */

    double time_period_s;               /**< Budget period (s) */
    uint32_t module_id;                 /**< Source module */

    uint64_t timestamp_us;              /**< Report timestamp */
} thermo_bio_energy_budget_msg_t;

/**
 * @brief Landauer cost message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint64_t bits_erased;               /**< Total bits erased */
    double minimum_cost;                /**< Landauer limit (J) */
    double actual_cost;                 /**< Actual energy used (J) */
    double efficiency;                  /**< min/actual [0, 1] */

    double cost_per_bit;                /**< Current J/bit */
    double landauer_limit_per_bit;      /**< Theoretical min J/bit */

    uint32_t module_id;                 /**< Source module */
    uint64_t timestamp_us;              /**< Report timestamp */
} thermo_bio_landauer_cost_msg_t;

/**
 * @brief Efficiency message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    double computational_efficiency;    /**< Useful work / total [0, 1] */
    double thermodynamic_efficiency;    /**< Actual / Carnot [0, 1] */
    double landauer_efficiency;         /**< Actual / Landauer [0, 1] */
    double energy_per_bit;              /**< Current J/bit */

    double useful_work;                 /**< Work output (J) */
    double total_energy;                /**< Total energy (J) */

    uint32_t module_id;                 /**< Source module */
    uint64_t timestamp_us;              /**< Report timestamp */
} thermo_bio_efficiency_msg_t;

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
} thermo_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Thermodynamics bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t broadcast_interval_ms;          /**< State broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox msgs per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t alert_channel;   /**< Channel for alerts */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum subscriptions */

    /* Alert thresholds */
    double atp_warning_threshold;            /**< ATP warning level [0, 1] */
    double atp_critical_threshold;           /**< ATP critical level [0, 1] */

    /* Feature flags */
    bool enable_atp_alerts;                  /**< Enable ATP alerts */
    bool enable_entropy_broadcast;           /**< Broadcast entropy */
    bool enable_efficiency_broadcast;        /**< Broadcast efficiency */
    bool enable_landauer_tracking;           /**< Track Landauer costs */
    bool enable_logging;                     /**< Enable message logging */
} thermo_bio_async_config_t;

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
    uint64_t messages_dropped;               /**< Messages dropped */
    uint64_t broadcasts_sent;                /**< Broadcast messages */

    /* Per-type counts */
    uint64_t temperature_broadcasts;         /**< Temperature broadcasts */
    uint64_t atp_broadcasts;                 /**< ATP level broadcasts */
    uint64_t atp_alerts_sent;                /**< ATP critical alerts */
    uint64_t entropy_broadcasts;             /**< Entropy broadcasts */
    uint64_t efficiency_broadcasts;          /**< Efficiency broadcasts */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active */
    uint32_t peak_subscriptions;             /**< Peak count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast */
    float avg_message_latency_us;            /**< Average latency */

    /* Error counts */
    uint64_t handler_errors;                 /**< Handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} thermo_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Thermodynamics bio-async bridge handle
 */
typedef struct thermo_bio_async_bridge_struct thermo_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int thermo_bio_async_default_config(thermo_bio_async_config_t* config);

/**
 * @brief Create thermodynamics bio-async bridge
 */
thermo_bio_async_bridge_t* thermo_bio_async_bridge_create(
    const thermo_bio_async_config_t* config
);

/**
 * @brief Destroy thermodynamics bio-async bridge
 */
void thermo_bio_async_bridge_destroy(thermo_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to thermodynamic state and router
 */
int thermo_bio_async_connect(
    thermo_bio_async_bridge_t* bridge,
    nimcp_thermodynamic_state_t* state,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 */
int thermo_bio_async_disconnect(thermo_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 */
bool thermo_bio_async_is_connected(const thermo_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 */
int thermo_bio_async_process_inbox(
    thermo_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 */
int thermo_bio_async_update(
    thermo_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast temperature state
 */
int thermo_bio_async_broadcast_temperature(
    thermo_bio_async_bridge_t* bridge,
    double temperature_k
);

/**
 * @brief Broadcast heat flow
 */
int thermo_bio_async_broadcast_heat_flow(
    thermo_bio_async_bridge_t* bridge,
    double heat_dissipation,
    double heat_absorbed
);

/**
 * @brief Broadcast entropy production
 */
int thermo_bio_async_broadcast_entropy(
    thermo_bio_async_bridge_t* bridge
);

/**
 * @brief Broadcast ATP level
 */
int thermo_bio_async_broadcast_atp_level(
    thermo_bio_async_bridge_t* bridge
);

/**
 * @brief Send ATP critical alert
 */
int thermo_bio_async_send_atp_critical(
    thermo_bio_async_bridge_t* bridge,
    uint32_t severity
);

/**
 * @brief Broadcast energy budget
 */
int thermo_bio_async_broadcast_energy_budget(
    thermo_bio_async_bridge_t* bridge,
    const nimcp_energy_budget_t* budget
);

/**
 * @brief Broadcast Landauer cost
 */
int thermo_bio_async_broadcast_landauer_cost(
    thermo_bio_async_bridge_t* bridge,
    const nimcp_landauer_cost_t* cost
);

/**
 * @brief Broadcast efficiency metrics
 */
int thermo_bio_async_broadcast_efficiency(
    thermo_bio_async_bridge_t* bridge
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to thermodynamics messages
 */
int thermo_bio_async_subscribe_module(
    thermo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from thermodynamics messages
 */
int thermo_bio_async_unsubscribe_module(
    thermo_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 */
int thermo_bio_async_update_subscription(
    thermo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 */
uint32_t thermo_bio_async_get_subscriber_count(
    const thermo_bio_async_bridge_t* bridge,
    thermo_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int thermo_bio_async_get_stats(
    const thermo_bio_async_bridge_t* bridge,
    thermo_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 */
int thermo_bio_async_reset_stats(thermo_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 */
const char* thermo_bio_msg_type_name(thermo_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 */
void thermo_bio_async_print_summary(const thermo_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_BIO_ASYNC_BRIDGE_H */
