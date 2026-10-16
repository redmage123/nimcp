/**
 * @file nimcp_claustrum_bio_async_bridge.h
 * @brief Claustrum Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for Claustrum consciousness binding module
 *       that provides comprehensive message routing for consciousness state via bio-router.
 *
 * WHY: The Claustrum needs to communicate:
 *      - Cross-modal binding events to downstream processors
 *      - Consciousness level changes for global workspace coordination
 *      - Synchronization signals for temporal binding
 *      - Salience detection for attention coordination
 *      - Attention switch events for task switching
 *      - Modality gating signals for sensory filtering
 *
 * HOW: Registers Claustrum as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming modulation.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * CLAUSTRUM OUTPUT SIGNALS:
 * -------------------------
 * 1. Binding events:
 *    - Cross-modal feature binding results
 *    - Unified percept formation
 *    - Mapped to: CLAUSTRUM_BIO_MSG_BINDING
 *
 * 2. Consciousness level:
 *    - Global workspace access state
 *    - Consciousness level transitions
 *    - Mapped to: CLAUSTRUM_BIO_MSG_CONSCIOUSNESS_CHANGE
 *
 * 3. Synchronization signals:
 *    - Gamma oscillation synchronization
 *    - Temporal binding coordination
 *    - Mapped to: CLAUSTRUM_BIO_MSG_SYNC
 *
 * CLAUSTRUM INPUT SIGNALS:
 * ------------------------
 * 1. Sensory modality updates:
 *    - Visual, auditory, somatosensory inputs
 *    - Feature vectors for binding
 *
 * 2. Attention modulation:
 *    - Top-down attention bias
 *    - Executive control signals
 *
 * 3. Salience signals:
 *    - Bottom-up salience from sensory systems
 *    - Priority signals from emotion/motivation
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

#ifndef NIMCP_CLAUSTRUM_BIO_ASYNC_BRIDGE_H
#define NIMCP_CLAUSTRUM_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/claustrum/nimcp_claustrum.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for Claustrum */
#define BIO_MODULE_CLAUSTRUM                0x5002

/** Maximum number of module subscriptions */
#define CLAUSTRUM_BIO_BRIDGE_MAX_SUBSCRIPTIONS     64

/** Maximum pending messages in inbox */
#define CLAUSTRUM_BIO_BRIDGE_MAX_INBOX_SIZE        256

/** Maximum pending messages in outbox */
#define CLAUSTRUM_BIO_BRIDGE_MAX_OUTBOX_SIZE       128

/** Default broadcast interval for state updates (ms) */
#define CLAUSTRUM_BIO_BRIDGE_DEFAULT_INTERVAL_MS   50

/** Message expiry time (ms) */
#define CLAUSTRUM_BIO_BRIDGE_MESSAGE_TTL_MS        5000

/** Default binding coherence threshold */
#define CLAUSTRUM_BIO_BRIDGE_BINDING_THRESHOLD     0.6f

/** Default salience threshold for alerts */
#define CLAUSTRUM_BIO_BRIDGE_SALIENCE_THRESHOLD    0.5f

/*
 * NOTE: Message types are defined in nimcp_claustrum.h as nimcp_claustrum_bio_msg_type_t:
 *   CLAUSTRUM_BIO_MSG_BINDING              - Cross-modal binding result
 *   CLAUSTRUM_BIO_MSG_SYNC                 - Synchronization signal
 *   CLAUSTRUM_BIO_MSG_SALIENCE             - Salience detection result
 *   CLAUSTRUM_BIO_MSG_ATTENTION_BIAS       - Attention modulation signal
 *   CLAUSTRUM_BIO_MSG_STATE_SWITCH         - Brain state switch notification
 *   CLAUSTRUM_BIO_MSG_WORKSPACE_GATE       - Global workspace gating
 *   CLAUSTRUM_BIO_MSG_PERCEPT_BROADCAST    - Unified percept broadcast
 *   CLAUSTRUM_BIO_MSG_GAMMA_MODULATION     - Gamma oscillation control
 *   CLAUSTRUM_BIO_MSG_ALPHA_MODULATION     - Alpha oscillation control
 *   CLAUSTRUM_BIO_MSG_REQUEST_BINDING      - Binding request from module
 *   CLAUSTRUM_BIO_MSG_MODALITY_UPDATE      - Sensory modality update
 *   CLAUSTRUM_BIO_MSG_CONSCIOUSNESS_CHANGE - Consciousness level change
 *
 * Subscription bitmasks are also in nimcp_claustrum.h:
 *   CLAUSTRUM_BIO_SUB_BINDING, CLAUSTRUM_BIO_SUB_SYNC, etc.
 */

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Feature binding message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    /* Binding information */
    uint32_t percept_id;                    /**< Unique bound percept identifier */
    uint32_t modality_mask;                 /**< Bitmask of bound modalities */
    uint32_t num_modalities;                /**< Number of bound modalities */

    /* Binding quality */
    float binding_strength;                 /**< Binding strength [0, 1] */
    float coherence;                        /**< Temporal coherence [0, 1] */
    float stability;                        /**< Percept stability [0, 1] */

    /* Consciousness status */
    uint32_t consciousness_level;           /**< nimcp_claustrum_consciousness_level_t */
    bool in_workspace;                      /**< Currently in global workspace */
    float access_strength;                  /**< Global workspace access [0, 1] */

    uint64_t timestamp_us;                  /**< Binding event timestamp */
} claustrum_bio_binding_msg_t;

/**
 * @brief Consciousness level change message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    /* Level transition */
    uint32_t old_level;                     /**< Previous consciousness level */
    uint32_t new_level;                     /**< New consciousness level */

    /* Associated percept */
    uint32_t percept_id;                    /**< Percept involved in transition */
    float access_strength;                  /**< Current workspace access [0, 1] */

    /* Global workspace state */
    bool workspace_occupied;                /**< Is workspace currently occupied */
    float global_coherence;                 /**< Global oscillatory coherence */

    uint64_t timestamp_us;                  /**< Transition timestamp */
} claustrum_bio_consciousness_msg_t;

/**
 * @brief Synchronization signal message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    /* Oscillation state */
    float gamma_frequency;                  /**< Current gamma frequency (Hz) */
    float gamma_amplitude;                  /**< Gamma amplitude [0, 1] */
    float gamma_phase;                      /**< Current gamma phase (rad) */

    float alpha_frequency;                  /**< Current alpha frequency (Hz) */
    float alpha_amplitude;                  /**< Alpha amplitude [0, 1] */
    float alpha_phase;                      /**< Current alpha phase (rad) */

    /* Coherence metrics */
    float global_coherence;                 /**< Overall oscillatory coherence [0, 1] */
    float binding_coherence;                /**< Binding-specific coherence [0, 1] */
    float phase_amplitude_coupling;         /**< Gamma-alpha coupling */

    uint64_t timestamp_us;                  /**< Sync signal timestamp */
} claustrum_bio_sync_msg_t;

/**
 * @brief Salience detection message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    /* Salience information */
    float global_salience;                  /**< Overall salience [0, 1] */
    uint32_t salient_modality;              /**< Most salient modality */

    /* Salience breakdown by modality */
    float modality_salience[8];             /**< Per-modality salience array */
    uint32_t active_modality_mask;          /**< Bitmask of active modalities */

    /* Attention influence */
    float novelty_contribution;             /**< Novelty component [0, 1] */
    float relevance_contribution;           /**< Relevance component [0, 1] */
    float intensity_contribution;           /**< Intensity component [0, 1] */

    uint64_t timestamp_us;                  /**< Detection timestamp */
} claustrum_bio_salience_msg_t;

/**
 * @brief Attention switch event message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    /* State transition */
    uint32_t old_brain_state;               /**< Previous brain state */
    uint32_t new_brain_state;               /**< New brain state */

    /* Switch details */
    float switch_progress;                  /**< Progress through switch [0, 1] */
    float switch_duration_ms;               /**< Total switch duration */
    bool switch_complete;                   /**< Whether switch is complete */

    /* Trigger information */
    uint32_t trigger_modality;              /**< Modality that triggered switch */
    float trigger_salience;                 /**< Salience that triggered switch */

    uint64_t timestamp_us;                  /**< Switch event timestamp */
} claustrum_bio_attention_switch_msg_t;

/**
 * @brief Modality gating signal message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    /* Gating information */
    uint32_t modality;                      /**< Target modality */
    float gate_level;                       /**< Gating level [0=closed, 1=open] */
    bool gate_open;                         /**< Boolean gate state */

    /* Attention bias */
    float attention_bias;                   /**< Attention bias for modality [0, 1] */
    uint32_t source_region;                 /**< Requesting cortical region */

    /* Alpha power modulation */
    float alpha_suppression;                /**< Alpha suppression level [0, 1] */

    uint64_t timestamp_us;                  /**< Gating timestamp */
} claustrum_bio_modality_gate_msg_t;

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
} claustrum_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Claustrum bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t broadcast_interval_ms;         /**< State broadcast interval */
    bool enable_auto_broadcast;             /**< Auto-broadcast state changes */
    bool enable_binding_broadcast;          /**< Broadcast on binding events */

    /* Message handling */
    uint32_t max_inbox_process_per_update;  /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                /**< Message time-to-live */

    /* Priority settings */
    float binding_threshold;                /**< Binding coherence threshold */
    float salience_threshold;               /**< Salience alert threshold */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t binding_channel; /**< Channel for binding events */

    /* Subscription limits */
    uint32_t max_subscriptions;             /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_consciousness_broadcast;    /**< Broadcast consciousness changes */
    bool enable_sync_broadcast;             /**< Broadcast sync signals */
    bool enable_salience_alerts;            /**< Enable salience alerts */
    bool enable_attention_events;           /**< Enable attention switch events */
    bool enable_gating_broadcast;           /**< Broadcast modality gating */
    bool enable_logging;                    /**< Enable message logging */
} claustrum_bio_async_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                 /**< Total messages sent */
    uint64_t messages_received;             /**< Total messages received */
    uint64_t messages_dropped;              /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;               /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t binding_broadcasts;            /**< Binding event broadcasts */
    uint64_t consciousness_broadcasts;      /**< Consciousness change broadcasts */
    uint64_t sync_broadcasts;               /**< Sync signal broadcasts */
    uint64_t salience_broadcasts;           /**< Salience detection broadcasts */
    uint64_t attention_switch_broadcasts;   /**< Attention switch broadcasts */
    uint64_t modality_gate_broadcasts;      /**< Modality gating broadcasts */

    /* Subscription stats */
    uint32_t active_subscriptions;          /**< Currently active subs */
    uint32_t peak_subscriptions;            /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;        /**< Last broadcast timestamp */
    float avg_message_latency_us;           /**< Average message latency */
    float max_message_latency_us;           /**< Peak message latency */

    /* Error counts */
    uint64_t handler_errors;                /**< Message handler errors */
    uint64_t routing_errors;                /**< Routing failures */
} claustrum_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure (Opaque)
 * ============================================================================ */

/**
 * @brief Claustrum bio-async bridge handle
 */
typedef struct claustrum_bio_async_bridge_struct claustrum_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_default_config(claustrum_bio_async_config_t* config);

/**
 * @brief Create Claustrum bio-async bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
claustrum_bio_async_bridge_t* claustrum_bio_async_bridge_create(
    const claustrum_bio_async_config_t* config
);

/**
 * @brief Destroy Claustrum bio-async bridge
 *
 * @param bridge Bridge to destroy
 */
void claustrum_bio_async_bridge_destroy(claustrum_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to Claustrum module and router
 *
 * @param bridge Bridge handle
 * @param claustrum Claustrum module to connect
 * @param router Bio-router for message dispatch
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_connect(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_t* claustrum,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_disconnect(claustrum_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool claustrum_bio_async_is_connected(const claustrum_bio_async_bridge_t* bridge);

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
int claustrum_bio_async_process_inbox(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_update(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast feature binding event
 *
 * @param bridge Bridge handle
 * @param percept Bound percept to broadcast
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_broadcast_binding(
    claustrum_bio_async_bridge_t* bridge,
    const nimcp_claustrum_bound_percept_t* percept
);

/**
 * @brief Broadcast consciousness level change
 *
 * @param bridge Bridge handle
 * @param old_level Previous consciousness level
 * @param new_level New consciousness level
 * @param percept_id Associated percept ID
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_broadcast_consciousness(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_consciousness_level_t old_level,
    nimcp_claustrum_consciousness_level_t new_level,
    uint32_t percept_id
);

/**
 * @brief Broadcast synchronization signal
 *
 * @param bridge Bridge handle
 * @param oscillator Oscillator state to broadcast
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_broadcast_sync(
    claustrum_bio_async_bridge_t* bridge,
    const nimcp_claustrum_oscillator_t* oscillator
);

/**
 * @brief Broadcast salience detection
 *
 * @param bridge Bridge handle
 * @param global_salience Overall salience level
 * @param salient_modality Most salient modality
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_broadcast_salience(
    claustrum_bio_async_bridge_t* bridge,
    float global_salience,
    nimcp_claustrum_modality_t salient_modality
);

/**
 * @brief Broadcast attention switch event
 *
 * @param bridge Bridge handle
 * @param old_state Previous brain state
 * @param new_state New brain state
 * @param progress Switch progress [0, 1]
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_broadcast_attention_switch(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_brain_state_t old_state,
    nimcp_claustrum_brain_state_t new_state,
    float progress
);

/**
 * @brief Broadcast modality gating signal
 *
 * @param bridge Bridge handle
 * @param modality Target modality
 * @param gate_level Gating level [0=closed, 1=open]
 * @param attention_bias Attention bias for modality
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_broadcast_modality_gate(
    claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_modality_t modality,
    float gate_level,
    float attention_bias
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to Claustrum messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (use CLAUSTRUM_BIO_SUB_* macros)
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_subscribe_module(
    claustrum_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from Claustrum messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_unsubscribe_module(
    claustrum_bio_async_bridge_t* bridge,
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
int claustrum_bio_async_update_subscription(
    claustrum_bio_async_bridge_t* bridge,
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
uint32_t claustrum_bio_async_get_subscriber_count(
    const claustrum_bio_async_bridge_t* bridge,
    nimcp_claustrum_bio_msg_type_t msg_type
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
int claustrum_bio_async_get_stats(
    const claustrum_bio_async_bridge_t* bridge,
    claustrum_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int claustrum_bio_async_reset_stats(claustrum_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Static string name
 */
const char* claustrum_bio_msg_type_name(nimcp_claustrum_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void claustrum_bio_async_print_summary(const claustrum_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CLAUSTRUM_BIO_ASYNC_BRIDGE_H */
