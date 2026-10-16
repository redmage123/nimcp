//=============================================================================
// nimcp_ephaptic_bio_async_bridge.h - Ephaptic Bio-Async Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_bio_async_bridge.h
 * @brief Bio-async messaging bridge for Ephaptic coupling module (AC-1)
 *
 * WHAT: Provides asynchronous messaging integration for the Ephaptic coupling
 *       module, enabling field state, synchronization, and LFP data to be
 *       broadcast to subscribed systems.
 *
 * WHY:  Neural field effects and synchronization need to communicate with:
 *       - Cognitive layer (oscillation-based attention)
 *       - Hypothalamus (EEG pattern detection)
 *       - SNN/Plasticity (phase-dependent plasticity)
 *       - Hodgkin-Huxley (field-induced modulation)
 *       - Perception layer (binding via synchrony)
 *
 * HOW:  - Wraps ephaptic data in bio-async message payloads
 *       - Routes via central bio_router_t
 *       - Supports subscription-based selective delivery
 *       - Uses neuromodulator channels for priority routing
 *
 * BIOLOGICAL: Ephaptic fields provide rapid, non-synaptic coordination.
 * This bridge allows the collective field state to influence higher-level
 * processing without direct synaptic connectivity.
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_BIO_ASYNC_BRIDGE_H
#define NIMCP_EPHAPTIC_BIO_ASYNC_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Identification
//=============================================================================

/** Module ID for Ephaptic in bio-async system */
#define BIO_MODULE_PHYSICS_EPHAPTIC     0x4502

/** Message type range for Ephaptic (0x1340-0x135F) */
#define EPHAPTIC_BIO_MSG_BASE           0x1340

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of concurrent subscriptions */
#define EPHAPTIC_BIO_MAX_SUBSCRIPTIONS  64

/** Default broadcast interval for auto-broadcast mode (ms) */
#define EPHAPTIC_BIO_DEFAULT_INTERVAL   50

/** Default message time-to-live (ms) */
#define EPHAPTIC_BIO_DEFAULT_TTL        200

/** Maximum inbox messages to process per update cycle */
#define EPHAPTIC_BIO_MAX_INBOX_PER_UPDATE 32

//=============================================================================
// Message Types
//=============================================================================

/**
 * @brief Ephaptic bio-async message types
 *
 * BIOLOGICAL: Each message type corresponds to a different aspect of
 * ephaptic field dynamics and neural synchronization.
 */
typedef enum {
    /** Electric field state (E-field components) */
    EPHAPTIC_BIO_MSG_FIELD_STATE = 0,

    /** Coupling strength update */
    EPHAPTIC_BIO_MSG_COUPLING_STRENGTH,

    /** Phase synchronization (Kuramoto order parameter) */
    EPHAPTIC_BIO_MSG_SYNC_PHASE,

    /** Spectral band power (delta, theta, alpha, beta, gamma) */
    EPHAPTIC_BIO_MSG_BAND_POWER,

    /** Oscillation frequency detected */
    EPHAPTIC_BIO_MSG_OSCILLATION,

    /** Ionic concentration gradient */
    EPHAPTIC_BIO_MSG_IONIC_GRADIENT,

    /** Spatial field pattern (direction, magnitude) */
    EPHAPTIC_BIO_MSG_SPATIAL_PATTERN,

    /** LFP amplitude and phase */
    EPHAPTIC_BIO_MSG_LFP_STATE,

    /** Number of message types */
    EPHAPTIC_BIO_MSG_COUNT
} ephaptic_bio_msg_type_t;

//=============================================================================
// Subscription Bitmasks
//=============================================================================

/** Subscribe to field state messages */
#define EPHAPTIC_BIO_SUB_FIELD_STATE       (1U << EPHAPTIC_BIO_MSG_FIELD_STATE)

/** Subscribe to coupling strength messages */
#define EPHAPTIC_BIO_SUB_COUPLING_STRENGTH (1U << EPHAPTIC_BIO_MSG_COUPLING_STRENGTH)

/** Subscribe to sync phase messages */
#define EPHAPTIC_BIO_SUB_SYNC_PHASE        (1U << EPHAPTIC_BIO_MSG_SYNC_PHASE)

/** Subscribe to band power messages */
#define EPHAPTIC_BIO_SUB_BAND_POWER        (1U << EPHAPTIC_BIO_MSG_BAND_POWER)

/** Subscribe to oscillation messages */
#define EPHAPTIC_BIO_SUB_OSCILLATION       (1U << EPHAPTIC_BIO_MSG_OSCILLATION)

/** Subscribe to ionic gradient messages */
#define EPHAPTIC_BIO_SUB_IONIC_GRADIENT    (1U << EPHAPTIC_BIO_MSG_IONIC_GRADIENT)

/** Subscribe to spatial pattern messages */
#define EPHAPTIC_BIO_SUB_SPATIAL_PATTERN   (1U << EPHAPTIC_BIO_MSG_SPATIAL_PATTERN)

/** Subscribe to LFP state messages */
#define EPHAPTIC_BIO_SUB_LFP_STATE         (1U << EPHAPTIC_BIO_MSG_LFP_STATE)

/** Subscribe to all ephaptic messages */
#define EPHAPTIC_BIO_SUB_ALL               (0xFFFFFFFFU)

//=============================================================================
// Message Payload Structures
//=============================================================================

/**
 * @brief Field state message payload
 *
 * BIOLOGICAL: Electric field in extracellular space (V/m)
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST - required by bio-async */
    float field_x;                      /**< E-field X component (V/m) */
    float field_y;                      /**< E-field Y component (V/m) */
    float field_z;                      /**< E-field Z component (V/m) */
    float field_magnitude;              /**< Total field magnitude (V/m) */
    float field_potential;              /**< Local field potential (mV) */
    uint64_t timestamp_us;              /**< Timestamp in microseconds */
} ephaptic_bio_field_state_msg_t;

/**
 * @brief Coupling strength message payload
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST */
    float coupling_strength;            /**< Current coupling strength [0,1] */
    float kuramoto_coupling;            /**< Kuramoto K parameter */
    bool adaptive_enabled;              /**< Is adaptive coupling active */
    uint64_t timestamp_us;
} ephaptic_bio_coupling_msg_t;

/**
 * @brief Phase synchronization message payload
 *
 * BIOLOGICAL: Kuramoto order parameter indicates collective phase coherence
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST */
    float order_parameter;              /**< Kuramoto r in [0,1] */
    float mean_phase;                   /**< Mean population phase (radians) */
    uint32_t synced_neuron_count;       /**< Number of phase-locked neurons */
    uint32_t total_neuron_count;        /**< Total tracked neurons */
    float sync_threshold;               /**< Current sync detection threshold */
    uint64_t timestamp_us;
} ephaptic_bio_sync_phase_msg_t;

/**
 * @brief Spectral band power message payload
 *
 * BIOLOGICAL: Power in standard EEG frequency bands reflects
 * cognitive state and arousal level.
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST */
    float delta_power;                  /**< 1-4 Hz (deep sleep, unconsciousness) */
    float theta_power;                  /**< 4-8 Hz (meditation, memory encoding) */
    float alpha_power;                  /**< 8-12 Hz (relaxed wakefulness) */
    float beta_power;                   /**< 12-30 Hz (active thinking) */
    float gamma_power;                  /**< 30+ Hz (binding, attention) */
    float dominant_band;                /**< Index of dominant band (0-4) */
    float total_power;                  /**< Total spectral power */
    uint64_t timestamp_us;
} ephaptic_bio_band_power_msg_t;

/**
 * @brief Oscillation frequency message payload
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST */
    float dominant_frequency;           /**< Dominant oscillation frequency (Hz) */
    float frequency_stability;          /**< How stable is the oscillation [0,1] */
    float amplitude;                    /**< Oscillation amplitude (mV) */
    uint64_t timestamp_us;
} ephaptic_bio_oscillation_msg_t;

/**
 * @brief Ionic gradient message payload
 *
 * BIOLOGICAL: Ionic concentration gradients affect field strength and direction
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST */
    float gradient_x;                   /**< Gradient X component */
    float gradient_y;                   /**< Gradient Y component */
    float gradient_z;                   /**< Gradient Z component */
    float gradient_magnitude;           /**< Total gradient magnitude */
    float extracellular_resistivity;    /**< Current resistivity (Ohm-cm) */
    uint64_t timestamp_us;
} ephaptic_bio_ionic_gradient_msg_t;

/**
 * @brief Spatial field pattern message payload
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST */
    float direction_x;                  /**< Normalized field direction X */
    float direction_y;                  /**< Normalized field direction Y */
    float direction_z;                  /**< Normalized field direction Z */
    float spatial_extent;               /**< Extent of field influence (mm) */
    float uniformity;                   /**< Field uniformity [0,1] */
    uint64_t timestamp_us;
} ephaptic_bio_spatial_pattern_msg_t;

/**
 * @brief LFP state message payload
 *
 * BIOLOGICAL: Local field potential reflects summed neural activity
 */
typedef struct {
    bio_message_header_t header;        /**< ALWAYS FIRST */
    float lfp_amplitude;                /**< LFP amplitude (mV) */
    float lfp_phase;                    /**< LFP phase (radians) */
    float lfp_accumulated;              /**< Accumulated LFP for decay */
    float position[3];                  /**< Recording position (mm) */
    uint64_t timestamp_us;
} ephaptic_bio_lfp_state_msg_t;

//=============================================================================
// Bridge Configuration
//=============================================================================

/**
 * @brief Ephaptic bio-async bridge configuration
 */
typedef struct {
    /** Broadcast interval for auto-broadcast mode (ms) */
    uint32_t broadcast_interval_ms;

    /** Enable automatic periodic broadcasting */
    bool enable_auto_broadcast;

    /** Maximum inbox messages to process per update */
    uint32_t max_inbox_per_update;

    /** Message time-to-live (ms) */
    uint32_t message_ttl_ms;

    /** Default neuromodulator channel for routing */
    nimcp_bio_channel_type_t default_channel;

    /** Phase coherence threshold for sync event broadcast */
    float sync_event_threshold;

    /** Minimum band power change to trigger broadcast */
    float band_power_delta_threshold;
} ephaptic_bio_async_config_t;

//=============================================================================
// Bridge Statistics
//=============================================================================

/**
 * @brief Statistics for ephaptic bio-async bridge
 */
typedef struct {
    /** Total messages sent */
    uint64_t messages_sent;

    /** Total messages received */
    uint64_t messages_received;

    /** Messages dropped (routing failures) */
    uint64_t messages_dropped;

    /** Field state broadcasts */
    uint64_t field_state_broadcasts;

    /** Sync phase broadcasts */
    uint64_t sync_phase_broadcasts;

    /** Band power broadcasts */
    uint64_t band_power_broadcasts;

    /** LFP state broadcasts */
    uint64_t lfp_state_broadcasts;

    /** Oscillation broadcasts */
    uint64_t oscillation_broadcasts;

    /** Current subscriber count */
    uint32_t subscriber_count;

    /** Last broadcast timestamp (us) */
    uint64_t last_broadcast_us;

    /** Average message latency (us) */
    float avg_latency_us;
} ephaptic_bio_async_stats_t;

//=============================================================================
// Opaque Bridge Type
//=============================================================================

/**
 * @brief Opaque handle to ephaptic bio-async bridge
 */
typedef struct ephaptic_bio_async_bridge_struct ephaptic_bio_async_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config    Configuration structure to fill
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_default_config(
    ephaptic_bio_async_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic bio-async bridge
 *
 * WHAT: Allocates and initializes a new bridge instance
 * WHY:  Enable ephaptic system to communicate via bio-async
 * HOW:  Allocates internal structures, sets up subscription table
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
NIMCP_EXPORT ephaptic_bio_async_bridge_t* ephaptic_bio_async_bridge_create(
    const ephaptic_bio_async_config_t* config
);

/**
 * @brief Destroy ephaptic bio-async bridge
 *
 * @param bridge    Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_bio_async_bridge_destroy(
    ephaptic_bio_async_bridge_t* bridge
);

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Registers bridge with central message router
 * WHY:  Enable actual message delivery
 * HOW:  Registers module ID and message handlers
 *
 * @param bridge    Bridge instance
 * @param router    Bio-async router handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_connect(
    ephaptic_bio_async_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * @param bridge    Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_disconnect(
    ephaptic_bio_async_bridge_t* bridge
);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge    Bridge instance
 * @return true if connected to router
 */
NIMCP_EXPORT bool ephaptic_bio_async_is_connected(
    const ephaptic_bio_async_bridge_t* bridge
);

//=============================================================================
// Broadcast API
//=============================================================================

/**
 * @brief Broadcast field state
 *
 * @param bridge    Bridge instance
 * @param system    Ephaptic system to broadcast from
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_field_state(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
);

/**
 * @brief Broadcast coupling strength
 *
 * @param bridge    Bridge instance
 * @param system    Ephaptic system
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_coupling(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
);

/**
 * @brief Broadcast phase synchronization state
 *
 * @param bridge    Bridge instance
 * @param system    Ephaptic system
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_sync_phase(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
);

/**
 * @brief Broadcast spectral band power
 *
 * @param bridge    Bridge instance
 * @param lfp       LFP result containing band power
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_band_power(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_lfp_result_t* lfp
);

/**
 * @brief Broadcast oscillation frequency
 *
 * @param bridge    Bridge instance
 * @param lfp       LFP result containing oscillation data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_oscillation(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_lfp_result_t* lfp
);

/**
 * @brief Broadcast ionic gradient
 *
 * @param bridge        Bridge instance
 * @param gradient      Gradient components [x, y, z]
 * @param resistivity   Extracellular resistivity
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_ionic_gradient(
    ephaptic_bio_async_bridge_t* bridge,
    const float gradient[3],
    float resistivity
);

/**
 * @brief Broadcast spatial field pattern
 *
 * @param bridge    Bridge instance
 * @param system    Ephaptic system
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_spatial_pattern(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
);

/**
 * @brief Broadcast LFP state
 *
 * @param bridge    Bridge instance
 * @param system    Ephaptic system
 * @param lfp       LFP result
 * @param position  Recording position
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_lfp_state(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system,
    const nimcp_lfp_result_t* lfp,
    const float position[3]
);

/**
 * @brief Broadcast all ephaptic state (convenience function)
 *
 * @param bridge    Bridge instance
 * @param system    Ephaptic system
 * @param lfp       LFP result (optional, NULL to skip band power)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_broadcast_all(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system,
    const nimcp_lfp_result_t* lfp
);

//=============================================================================
// Subscription Management API
//=============================================================================

/**
 * @brief Subscribe a module to ephaptic messages
 *
 * @param bridge        Bridge instance
 * @param module_id     ID of subscribing module
 * @param message_mask  Bitmask of message types to receive
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_subscribe_module(
    ephaptic_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t message_mask
);

/**
 * @brief Unsubscribe a module from ephaptic messages
 *
 * @param bridge        Bridge instance
 * @param module_id     ID of module to unsubscribe
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_unsubscribe_module(
    ephaptic_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update subscription mask for existing subscriber
 *
 * @param bridge        Bridge instance
 * @param module_id     ID of subscribed module
 * @param message_mask  New message mask
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_update_subscription(
    ephaptic_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t message_mask
);

/**
 * @brief Get current subscriber count
 *
 * @param bridge    Bridge instance
 * @return Number of active subscribers, or 0 on error
 */
NIMCP_EXPORT uint32_t ephaptic_bio_async_get_subscriber_count(
    const ephaptic_bio_async_bridge_t* bridge
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge    Bridge instance
 * @param stats     Statistics output structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_get_stats(
    const ephaptic_bio_async_bridge_t* bridge,
    ephaptic_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge    Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_reset_stats(
    ephaptic_bio_async_bridge_t* bridge
);

//=============================================================================
// Update/Processing API
//=============================================================================

/**
 * @brief Process incoming messages
 *
 * WHAT: Processes messages from inbox
 * WHY:  Handle requests from other modules (e.g., sync queries)
 * HOW:  Reads from inbox, dispatches to handlers
 *
 * @param bridge    Bridge instance
 * @return Number of messages processed, or -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_process_inbox(
    ephaptic_bio_async_bridge_t* bridge
);

/**
 * @brief Set neuromodulator channel for next broadcast
 *
 * @param bridge    Bridge instance
 * @param channel   Channel type (dopamine, serotonin, etc.)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_bio_async_set_channel(
    ephaptic_bio_async_bridge_t* bridge,
    nimcp_bio_channel_type_t channel
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_BIO_ASYNC_BRIDGE_H */
