/**
 * @file nimcp_quantum_bio_async_bridge.h
 * @brief Quantum Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for quantum modules that provides
 *       message routing for coherence updates, entanglement events, quantum
 *       walks, and measurement results via the bio-router.
 *
 * WHY: Quantum computation modules need to coordinate with classical neural
 *      systems for hybrid quantum-classical processing:
 *      - Coherence state affects processing quality
 *      - Entanglement enables correlated computations
 *      - Measurement results drive classical decision-making
 *      - Annealing schedules require system-wide coordination
 *
 * HOW: Registers quantum modules as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes incoming
 *      quantum control requests.
 *
 * BIOLOGICAL BASIS (Quantum-Neural Interface):
 * =================================================================================
 *
 * QUANTUM OUTPUT PATHWAYS:
 * ------------------------
 * 1. Coherence Events:
 *    - Quantum coherence maintenance signals
 *    - Decoherence warnings and recovery
 *    - Mapped to: QUANTUM_BIO_MSG_COHERENCE_UPDATE
 *
 * 2. Entanglement Events:
 *    - Entanglement creation between qubits
 *    - Entanglement breaking/degradation
 *    - Bell state preparation results
 *    - Mapped to: QUANTUM_BIO_MSG_ENTANGLEMENT
 *
 * 3. Measurement Results:
 *    - Qubit measurement outcomes
 *    - Quantum state collapse events
 *    - Mapped to: QUANTUM_BIO_MSG_MEASUREMENT
 *
 * 4. Walk/Search Results:
 *    - Quantum walk propagation
 *    - Grover search amplification
 *    - MCTS-guided exploration
 *    - Mapped to: QUANTUM_BIO_MSG_WALK_UPDATE
 *
 * QUANTUM INPUT PATHWAYS:
 * -----------------------
 * 1. Control Signals:
 *    - Gate operation requests
 *    - Annealing schedule control
 *    - State preparation commands
 *
 * 2. Classical Data:
 *    - Input encoding for quantum processing
 *    - Variational parameter updates
 *
 * 3. Resource Management:
 *    - Qubit allocation requests
 *    - Error correction triggers
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

#ifndef NIMCP_QUANTUM_BIO_ASYNC_BRIDGE_H
#define NIMCP_QUANTUM_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define QUANTUM_BIO_MAX_SUBSCRIPTIONS       64

/** Maximum pending messages in inbox */
#define QUANTUM_BIO_MAX_INBOX_SIZE          256

/** Default broadcast interval for quantum state (ms) */
#define QUANTUM_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define QUANTUM_BIO_MESSAGE_TTL_MS          5000

/** Coherence warning threshold */
#define QUANTUM_BIO_COHERENCE_WARNING       0.3f

/** Critical decoherence threshold */
#define QUANTUM_BIO_COHERENCE_CRITICAL      0.1f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Quantum bio-async message types
 *
 * WHAT: Message type enumeration for quantum bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific quantum event or state change
 */
typedef enum {
    QUANTUM_BIO_MSG_COHERENCE_UPDATE = 0,   /**< Coherence level change */
    QUANTUM_BIO_MSG_ENTANGLEMENT,           /**< Entanglement event */
    QUANTUM_BIO_MSG_MEASUREMENT,            /**< Measurement result */
    QUANTUM_BIO_MSG_WALK_UPDATE,            /**< Quantum walk progress */
    QUANTUM_BIO_MSG_ANNEALING_STEP,         /**< Annealing schedule step */
    QUANTUM_BIO_MSG_GATE_APPLIED,           /**< Quantum gate applied */
    QUANTUM_BIO_MSG_ERROR_DETECTED,         /**< Quantum error detected */
    QUANTUM_BIO_MSG_ERROR_CORRECTED,        /**< Error correction applied */
    QUANTUM_BIO_MSG_STATE_PREPARED,         /**< State preparation complete */
    QUANTUM_BIO_MSG_AMPLITUDE_ESTIMATE,     /**< Amplitude estimation result */
    QUANTUM_BIO_MSG_REQUEST_QUBITS,         /**< Request qubit allocation */
    QUANTUM_BIO_MSG_QUERY_STATE,            /**< Query quantum state */
    QUANTUM_BIO_MSG_COUNT
} quantum_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define QUANTUM_BIO_SUB_COHERENCE_UPDATE    (1U << QUANTUM_BIO_MSG_COHERENCE_UPDATE)
#define QUANTUM_BIO_SUB_ENTANGLEMENT        (1U << QUANTUM_BIO_MSG_ENTANGLEMENT)
#define QUANTUM_BIO_SUB_MEASUREMENT         (1U << QUANTUM_BIO_MSG_MEASUREMENT)
#define QUANTUM_BIO_SUB_WALK_UPDATE         (1U << QUANTUM_BIO_MSG_WALK_UPDATE)
#define QUANTUM_BIO_SUB_ANNEALING_STEP      (1U << QUANTUM_BIO_MSG_ANNEALING_STEP)
#define QUANTUM_BIO_SUB_GATE_APPLIED        (1U << QUANTUM_BIO_MSG_GATE_APPLIED)
#define QUANTUM_BIO_SUB_ERROR_DETECTED      (1U << QUANTUM_BIO_MSG_ERROR_DETECTED)
#define QUANTUM_BIO_SUB_ERROR_CORRECTED     (1U << QUANTUM_BIO_MSG_ERROR_CORRECTED)
#define QUANTUM_BIO_SUB_STATE_PREPARED      (1U << QUANTUM_BIO_MSG_STATE_PREPARED)
#define QUANTUM_BIO_SUB_AMPLITUDE_ESTIMATE  (1U << QUANTUM_BIO_MSG_AMPLITUDE_ESTIMATE)
#define QUANTUM_BIO_SUB_ALL                 (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Coherence update message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    float coherence_level;                  /**< Overall coherence [0, 1] */
    float previous_coherence;               /**< Previous coherence level */
    float decoherence_rate;                 /**< Rate of decoherence */

    uint32_t num_qubits;                    /**< Number of qubits affected */
    float avg_t1_time_us;                   /**< Average T1 relaxation time */
    float avg_t2_time_us;                   /**< Average T2 dephasing time */

    bool is_warning;                        /**< Coherence below warning threshold */
    bool is_critical;                       /**< Coherence below critical threshold */

    uint64_t timestamp_us;                  /**< Measurement timestamp */
} quantum_bio_coherence_msg_t;

/**
 * @brief Entanglement event message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t qubit_a;                       /**< First entangled qubit */
    uint32_t qubit_b;                       /**< Second entangled qubit */
    float entanglement_fidelity;            /**< Entanglement quality [0, 1] */

    uint32_t bell_state;                    /**< Bell state type (0-3) */
    float concurrence;                      /**< Entanglement measure */

    bool is_creation;                       /**< true=created, false=broken */
    bool is_distilled;                      /**< Entanglement was distilled */

    uint32_t source_operation;              /**< Which operation created it */
    uint64_t timestamp_us;                  /**< Event timestamp */
} quantum_bio_entanglement_msg_t;

/**
 * @brief Measurement result message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t qubit_id;                      /**< Measured qubit */
    uint32_t basis;                         /**< Measurement basis (0=Z, 1=X, 2=Y) */
    int32_t outcome;                        /**< Measurement outcome (0 or 1) */

    float probability;                      /**< Probability of this outcome */
    float confidence;                       /**< Measurement confidence */

    uint32_t shot_index;                    /**< Which shot this is */
    uint32_t total_shots;                   /**< Total shots requested */

    uint64_t timestamp_us;                  /**< Measurement timestamp */
} quantum_bio_measurement_msg_t;

/**
 * @brief Quantum walk update message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t walk_id;                       /**< Walk instance ID */
    uint32_t step_number;                   /**< Current walk step */
    uint32_t total_steps;                   /**< Total steps planned */

    uint32_t current_position;              /**< Current walker position */
    float position_probability;             /**< Probability at current position */
    float spread;                           /**< Walk spread/diffusion */

    float amplitude_at_target;              /**< Amplitude at target state */
    bool target_found;                      /**< Target detected above threshold */

    uint64_t timestamp_us;                  /**< Walk timestamp */
} quantum_bio_walk_msg_t;

/**
 * @brief Annealing step message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t annealing_id;                  /**< Annealing instance ID */
    uint32_t step_number;                   /**< Current step */
    uint32_t total_steps;                   /**< Total annealing steps */

    float temperature;                      /**< Current temperature */
    float initial_temperature;              /**< Starting temperature */
    float final_temperature;                /**< Target temperature */

    float current_energy;                   /**< Current Hamiltonian energy */
    float best_energy;                      /**< Best energy found so far */

    bool is_complete;                       /**< Annealing finished */
    uint64_t timestamp_us;                  /**< Step timestamp */
} quantum_bio_annealing_msg_t;

/**
 * @brief Gate applied message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t gate_type;                     /**< Gate type (H, X, CNOT, etc.) */
    uint32_t target_qubit;                  /**< Target qubit */
    uint32_t control_qubit;                 /**< Control qubit (if two-qubit gate) */

    float gate_fidelity;                    /**< Gate application fidelity */
    float gate_time_us;                     /**< Gate duration */

    float rotation_angle;                   /**< Rotation angle (for rotation gates) */

    uint32_t layer_index;                   /**< Circuit layer index */
    uint64_t timestamp_us;                  /**< Application timestamp */
} quantum_bio_gate_msg_t;

/**
 * @brief Quantum error message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t error_type;                    /**< Error type (bit-flip, phase, etc.) */
    uint32_t qubit_id;                      /**< Affected qubit */
    float error_probability;                /**< Estimated error probability */

    bool is_correctable;                    /**< Can be corrected */
    bool was_corrected;                     /**< Correction applied */
    uint32_t correction_syndrome;           /**< Error syndrome */

    uint32_t error_count;                   /**< Total errors this run */
    uint64_t timestamp_us;                  /**< Detection timestamp */
} quantum_bio_error_msg_t;

/**
 * @brief Amplitude estimation result message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t target_state;                  /**< Target state index */
    float estimated_amplitude;              /**< Estimated |amplitude| */
    float estimated_probability;            /**< Estimated |amplitude|^2 */

    float variance;                         /**< Estimation variance */
    float std_error;                        /**< Standard error */
    uint32_t samples_used;                  /**< Number of samples */

    float effective_sample_size;            /**< ESS for importance sampling */
    uint64_t timestamp_us;                  /**< Estimation timestamp */
} quantum_bio_amplitude_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;              /**< Subscribed module ID */
    uint32_t msg_type_mask;                 /**< Bitmask of subscribed types */
    bool active;                            /**< Subscription active */
    uint64_t subscription_time;             /**< When subscribed */
    uint64_t messages_sent;                 /**< Messages sent to this sub */
} quantum_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Quantum bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t state_broadcast_interval_ms;    /**< State broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state changes */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t error_channel;   /**< Channel for error messages */

    /* Thresholds */
    float coherence_warning_threshold;        /**< Coherence warning level */
    float coherence_critical_threshold;       /**< Coherence critical level */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_coherence_routing;           /**< Route coherence updates */
    bool enable_entanglement_routing;        /**< Route entanglement events */
    bool enable_measurement_routing;         /**< Route measurement results */
    bool enable_error_routing;               /**< Route error events */
    bool enable_logging;                     /**< Enable message logging */
} quantum_bio_async_config_t;

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
    uint64_t coherence_updates_sent;         /**< Coherence update broadcasts */
    uint64_t entanglement_events_sent;       /**< Entanglement event messages */
    uint64_t measurements_sent;              /**< Measurement result messages */
    uint64_t walk_updates_sent;              /**< Quantum walk updates */
    uint64_t errors_detected;                /**< Error detection messages */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active subs */
    uint32_t peak_subscriptions;             /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;            /**< Average message latency */

    /* Quantum stats */
    float avg_coherence;                     /**< Average coherence level */
    float min_coherence;                     /**< Minimum coherence observed */
    uint64_t decoherence_warnings;           /**< Decoherence warning count */

    /* Error counts */
    uint64_t handler_errors;                 /**< Message handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} quantum_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Quantum bio-async bridge handle
 */
typedef struct quantum_bio_async_bridge_struct quantum_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int quantum_bio_async_default_config(quantum_bio_async_config_t* config);

/**
 * @brief Create quantum bio-async bridge
 */
quantum_bio_async_bridge_t* quantum_bio_async_bridge_create(
    const quantum_bio_async_config_t* config
);

/**
 * @brief Destroy quantum bio-async bridge
 */
void quantum_bio_async_bridge_destroy(quantum_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to router
 */
int quantum_bio_async_connect(
    quantum_bio_async_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 */
int quantum_bio_async_disconnect(quantum_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 */
bool quantum_bio_async_is_connected(const quantum_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 */
int quantum_bio_async_process_inbox(
    quantum_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 */
int quantum_bio_async_update(
    quantum_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast coherence update
 */
int quantum_bio_async_broadcast_coherence(
    quantum_bio_async_bridge_t* bridge,
    float coherence_level,
    uint32_t num_qubits
);

/**
 * @brief Broadcast entanglement event
 */
int quantum_bio_async_broadcast_entanglement(
    quantum_bio_async_bridge_t* bridge,
    uint32_t qubit_a,
    uint32_t qubit_b,
    float fidelity,
    bool is_creation
);

/**
 * @brief Broadcast measurement result
 */
int quantum_bio_async_broadcast_measurement(
    quantum_bio_async_bridge_t* bridge,
    uint32_t qubit_id,
    int32_t outcome,
    float probability
);

/**
 * @brief Broadcast quantum walk update
 */
int quantum_bio_async_broadcast_walk_update(
    quantum_bio_async_bridge_t* bridge,
    uint32_t walk_id,
    uint32_t step_number,
    float amplitude_at_target
);

/**
 * @brief Broadcast annealing step
 */
int quantum_bio_async_broadcast_annealing(
    quantum_bio_async_bridge_t* bridge,
    uint32_t annealing_id,
    uint32_t step_number,
    float temperature,
    float current_energy
);

/**
 * @brief Broadcast gate application
 */
int quantum_bio_async_broadcast_gate(
    quantum_bio_async_bridge_t* bridge,
    uint32_t gate_type,
    uint32_t target_qubit,
    float gate_fidelity
);

/**
 * @brief Broadcast quantum error
 */
int quantum_bio_async_broadcast_error(
    quantum_bio_async_bridge_t* bridge,
    uint32_t error_type,
    uint32_t qubit_id,
    bool is_correctable
);

/**
 * @brief Broadcast amplitude estimation result
 */
int quantum_bio_async_broadcast_amplitude(
    quantum_bio_async_bridge_t* bridge,
    uint32_t target_state,
    float estimated_amplitude,
    float variance
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to quantum messages
 */
int quantum_bio_async_subscribe_module(
    quantum_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from quantum messages
 */
int quantum_bio_async_unsubscribe_module(
    quantum_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Get subscription count for message type
 */
uint32_t quantum_bio_async_get_subscriber_count(
    const quantum_bio_async_bridge_t* bridge,
    quantum_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int quantum_bio_async_get_stats(
    const quantum_bio_async_bridge_t* bridge,
    quantum_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 */
int quantum_bio_async_reset_stats(quantum_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 */
const char* quantum_bio_msg_type_name(quantum_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 */
void quantum_bio_async_print_summary(const quantum_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_BIO_ASYNC_BRIDGE_H */
