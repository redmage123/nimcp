/**
 * @file nimcp_pag_bio_async_bridge.h
 * @brief Periaqueductal Gray (PAG) Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for PAG that provides comprehensive
 *       message routing for defensive behaviors, pain modulation, emotions,
 *       vocalization, and autonomic responses via bio-router.
 *
 * WHY: The PAG needs to communicate:
 *      - Defense state changes (fight/flight/freeze/fawn) to motor systems
 *      - Pain modulation signals to spinal cord and higher areas
 *      - Emotional state broadcasts for limbic integration
 *      - Vocalization triggers to motor nuclei
 *      - Autonomic responses to brainstem/hypothalamus
 *      - Threat level assessments for survival coordination
 *
 * HOW: Registers PAG as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * PAG OUTPUT SIGNALS:
 * ------------------
 * 1. Defense state changes:
 *    - Active coping (fight/flight) via dlPAG/lPAG
 *    - Passive coping (freeze/fawn) via vlPAG
 *    - Mapped to: PAG_BIO_MSG_DEFENSE_STATE
 *
 * 2. Pain modulation:
 *    - Descending inhibition to spinal cord
 *    - Opioid/non-opioid analgesia signals
 *    - Mapped to: PAG_BIO_MSG_PAIN_MOD
 *
 * 3. Emotional expression:
 *    - Fear, rage, panic, maternal behaviors
 *    - Mapped to: PAG_BIO_MSG_EMOTION
 *
 * 4. Vocalization:
 *    - Alarm, aggression, distress calls
 *    - Mapped to: PAG_BIO_MSG_VOCALIZATION
 *
 * 5. Autonomic responses:
 *    - Cardiovascular, respiratory changes
 *    - Mapped to: PAG_BIO_MSG_AUTONOMIC
 *
 * PAG INPUT SIGNALS:
 * -----------------
 * 1. Threat detection from amygdala, cortex
 * 2. Pain signals from spinal cord
 * 3. Drive signals from hypothalamus
 * 4. Inhibitory control from prefrontal cortex
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

#ifndef NIMCP_PAG_BIO_ASYNC_BRIDGE_H
#define NIMCP_PAG_BIO_ASYNC_BRIDGE_H

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

/** PAG module ID for bio-router registration */
#define PAG_BIO_MODULE_ID                   0x5003

/** Maximum number of module subscriptions */
#define PAG_BIO_MAX_SUBSCRIPTIONS           64

/** Maximum pending messages in inbox */
#define PAG_BIO_MAX_INBOX_SIZE              256

/** Maximum pending messages in outbox */
#define PAG_BIO_MAX_OUTBOX_SIZE             128

/** Default broadcast interval for state updates (ms) */
#define PAG_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define PAG_BIO_MESSAGE_TTL_MS              5000

/** Threat response latency threshold (us) */
#define PAG_BIO_URGENT_LATENCY_US           10000

/* ============================================================================
 * Message Types (PAG-specific bio-async messages)
 * ============================================================================ */

/**
 * @brief PAG bio-async message types
 *
 * WHAT: Message type enumeration for PAG bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific PAG output pathway
 */
typedef enum {
    PAG_BIO_MSG_DEFENSE_STATE = 0,      /**< Defense behavior activation */
    PAG_BIO_MSG_PAIN_MOD,               /**< Pain modulation/analgesia signal */
    PAG_BIO_MSG_EMOTION,                /**< Emotional state broadcast */
    PAG_BIO_MSG_VOCALIZATION,           /**< Vocalization trigger */
    PAG_BIO_MSG_AUTONOMIC,              /**< Autonomic response signal */
    PAG_BIO_MSG_THREAT_LEVEL,           /**< Threat detection level */
    PAG_BIO_MSG_COUNT
} pag_bio_bridge_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define PAG_BIO_BRIDGE_SUB_DEFENSE_STATE    (1U << PAG_BIO_MSG_DEFENSE_STATE)
#define PAG_BIO_BRIDGE_SUB_PAIN_MOD         (1U << PAG_BIO_MSG_PAIN_MOD)
#define PAG_BIO_BRIDGE_SUB_EMOTION          (1U << PAG_BIO_MSG_EMOTION)
#define PAG_BIO_BRIDGE_SUB_VOCALIZATION     (1U << PAG_BIO_MSG_VOCALIZATION)
#define PAG_BIO_BRIDGE_SUB_AUTONOMIC        (1U << PAG_BIO_MSG_AUTONOMIC)
#define PAG_BIO_BRIDGE_SUB_THREAT_LEVEL     (1U << PAG_BIO_MSG_THREAT_LEVEL)
#define PAG_BIO_BRIDGE_SUB_ALL              (0xFFFFFFFFU)

/* ============================================================================
 * Defense Types (for message payloads)
 * ============================================================================ */

/**
 * @brief Defense response types (4F model)
 */
typedef enum {
    PAG_BIO_DEFENSE_FIGHT = 0,          /**< Active confrontation */
    PAG_BIO_DEFENSE_FLIGHT,             /**< Active escape */
    PAG_BIO_DEFENSE_FREEZE,             /**< Passive immobility */
    PAG_BIO_DEFENSE_FAWN,               /**< Passive submission */
    PAG_BIO_DEFENSE_NONE                /**< No active defense */
} pag_bio_defense_type_t;

/**
 * @brief Threat proximity levels
 */
typedef enum {
    PAG_BIO_THREAT_NONE = 0,            /**< No threat detected */
    PAG_BIO_THREAT_DISTAL,              /**< Threat detected but distant */
    PAG_BIO_THREAT_PROXIMAL,            /**< Threat approaching */
    PAG_BIO_THREAT_IMMINENT,            /**< Threat immediate */
    PAG_BIO_THREAT_CONTACT              /**< Physical contact with threat */
} pag_bio_threat_level_t;

/**
 * @brief Vocalization types
 */
typedef enum {
    PAG_BIO_VOCAL_NONE = 0,
    PAG_BIO_VOCAL_ALARM,                /**< Warning/alarm calls */
    PAG_BIO_VOCAL_AGGRESSION,           /**< Aggressive vocalization */
    PAG_BIO_VOCAL_SUBMISSION,           /**< Submissive vocalization */
    PAG_BIO_VOCAL_DISTRESS,             /**< Pain/distress vocalization */
    PAG_BIO_VOCAL_PLEASURE              /**< Affiliative vocalization */
} pag_bio_vocal_type_t;

/**
 * @brief Emotion types
 */
typedef enum {
    PAG_BIO_EMOTION_FEAR = 0,           /**< Fear/terror */
    PAG_BIO_EMOTION_RAGE,               /**< Anger/rage */
    PAG_BIO_EMOTION_PAIN,               /**< Pain affect */
    PAG_BIO_EMOTION_PANIC,              /**< Panic/separation distress */
    PAG_BIO_EMOTION_MATERNAL,           /**< Maternal/nurturing */
    PAG_BIO_EMOTION_NEUTRAL             /**< Neutral state */
} pag_bio_emotion_type_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Defense state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Defense state */
    pag_bio_defense_type_t defense_type;/**< Current defense response */
    float defense_intensity;            /**< Response intensity [0, 1] */
    bool active_coping;                 /**< True = fight/flight, False = freeze/fawn */

    /* Threat context */
    pag_bio_threat_level_t threat_level;/**< Current threat proximity */
    float threat_intensity;             /**< Threat intensity [0, 1] */
    float threat_direction;             /**< Direction to threat (radians) */

    /* Column activity */
    float dlpag_activity;               /**< Dorsolateral PAG activity [0, 1] */
    float vlpag_activity;               /**< Ventrolateral PAG activity [0, 1] */

    /* Motor output */
    float motor_output;                 /**< Motor command strength [0, 1] */
    bool escape_route_available;        /**< Escape path exists */

    uint64_t timestamp_us;              /**< Event timestamp */
} pag_bio_defense_msg_t;

/**
 * @brief Pain modulation message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Analgesia state */
    float analgesia_level;              /**< Overall analgesia [0, 1] */
    float descending_inhibition;        /**< Signal to spinal cord [0, 1] */

    /* Pathway activity */
    float opioid_activity;              /**< Opioid pathway [0, 1] */
    float non_opioid_activity;          /**< Non-opioid pathway [0, 1] */
    float cannabinoid_activity;         /**< Cannabinoid pathway [0, 1] */
    float serotonergic_activity;        /**< Serotonergic pathway [0, 1] */

    /* Pain input */
    float pain_intensity;               /**< Input pain intensity [0, 1] */
    float pain_unpleasantness;          /**< Affective component [0, 1] */
    uint32_t pain_location;             /**< Body location encoding */

    /* Stress-induced analgesia */
    float stress_analgesia_factor;      /**< SIA contribution [0, 1] */
    bool opioid_tolerance;              /**< Tolerance developed */

    uint64_t timestamp_us;              /**< Event timestamp */
} pag_bio_pain_mod_msg_t;

/**
 * @brief Emotion state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Emotional state */
    pag_bio_emotion_type_t dominant;    /**< Dominant emotion */
    float emotional_intensity;          /**< Overall intensity [0, 1] */
    float valence;                      /**< Positive/negative [-1, +1] */
    float arousal;                      /**< Activation level [0, 1] */

    /* Per-emotion levels */
    float fear_level;                   /**< Fear [0, 1] */
    float rage_level;                   /**< Rage [0, 1] */
    float pain_affect;                  /**< Pain affect [0, 1] */
    float panic_level;                  /**< Panic [0, 1] */
    float maternal_level;               /**< Maternal [0, 1] */

    /* Context */
    uint32_t source_region;             /**< Source of emotion input */

    uint64_t timestamp_us;              /**< Event timestamp */
} pag_bio_emotion_msg_t;

/**
 * @brief Vocalization trigger message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Vocalization command */
    pag_bio_vocal_type_t vocal_type;    /**< Vocalization type */
    float intensity;                    /**< Vocalization intensity [0, 1] */
    float urgency;                      /**< Priority/urgency [0, 1] */
    float duration_ms;                  /**< Expected duration (ms) */

    /* Trigger context */
    pag_bio_defense_type_t defense_context; /**< Associated defense state */
    pag_bio_emotion_type_t emotion_context; /**< Associated emotion */

    /* Motor parameters */
    float pitch_modulation;             /**< Pitch modifier [-1, +1] */
    float volume_modulation;            /**< Volume modifier [0, 2] */

    bool is_active;                     /**< Currently vocalizing */

    uint64_t timestamp_us;              /**< Event timestamp */
} pag_bio_vocal_msg_t;

/**
 * @brief Autonomic response message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Cardiovascular */
    float heart_rate_mod;               /**< HR change [-1=brady, +1=tachy] */
    float blood_pressure_mod;           /**< BP change [-1, +1] */
    float vasoconstriction;             /**< Peripheral vasoconstriction [0, 1] */

    /* Respiratory */
    float respiratory_rate_mod;         /**< RR change [-1, +1] */
    float respiratory_depth_mod;        /**< Tidal volume change [-1, +1] */
    bool apnea_triggered;               /**< Breath-holding (freeze) */

    /* Other autonomic */
    float pupil_dilation;               /**< Mydriasis [0, 1] */
    float sweating;                     /**< Sudomotor response [0, 1] */
    float piloerection;                 /**< Hair standing [0, 1] */

    /* Muscle tone */
    float muscle_tone;                  /**< Global muscle tone [0, 1] */
    bool tonic_immobility;              /**< Complete freeze state */

    /* Source */
    pag_bio_defense_type_t defense_source;  /**< Triggering defense state */

    uint64_t timestamp_us;              /**< Event timestamp */
} pag_bio_autonomic_msg_t;

/**
 * @brief Threat level message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Threat assessment */
    pag_bio_threat_level_t level;       /**< Threat proximity level */
    float intensity;                    /**< Threat intensity [0, 1] */
    float direction;                    /**< Direction to threat (radians) */
    float distance;                     /**< Estimated distance */
    float velocity;                     /**< Threat approach velocity */

    /* Response recommendation */
    pag_bio_defense_type_t recommended; /**< Recommended defense response */
    float confidence;                   /**< Recommendation confidence [0, 1] */

    /* Context */
    bool escape_possible;               /**< Escape route available */
    bool fight_viable;                  /**< Fighting is viable option */
    uint32_t threat_source_id;          /**< Source identifier */

    /* Timing */
    uint64_t detection_time_us;         /**< When threat detected */
    float time_to_contact_ms;           /**< Estimated time to contact */

    uint64_t timestamp_us;              /**< Event timestamp */
} pag_bio_threat_msg_t;

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
} pag_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief PAG bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t broadcast_interval_ms;          /**< State broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state changes */
    bool enable_urgent_defense;              /**< Urgent flag for defense msgs */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Channel settings */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t defense_channel; /**< Channel for defense events */
    nimcp_bio_channel_type_t pain_channel;    /**< Channel for pain signals */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_defense_broadcast;           /**< Broadcast defense state */
    bool enable_pain_broadcast;              /**< Broadcast pain modulation */
    bool enable_emotion_broadcast;           /**< Broadcast emotional state */
    bool enable_vocal_broadcast;             /**< Broadcast vocalization */
    bool enable_autonomic_broadcast;         /**< Broadcast autonomic state */
    bool enable_threat_broadcast;            /**< Broadcast threat assessment */
    bool enable_logging;                     /**< Enable message logging */
} pag_bio_async_config_t;

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
    uint64_t defense_broadcasts;             /**< Defense state broadcasts */
    uint64_t pain_mod_broadcasts;            /**< Pain modulation broadcasts */
    uint64_t emotion_broadcasts;             /**< Emotion state broadcasts */
    uint64_t vocal_broadcasts;               /**< Vocalization broadcasts */
    uint64_t autonomic_broadcasts;           /**< Autonomic response broadcasts */
    uint64_t threat_broadcasts;              /**< Threat level broadcasts */

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
} pag_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief PAG bio-async bridge handle (opaque)
 */
typedef struct pag_bio_async_bridge_struct pag_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int pag_bio_async_default_config(pag_bio_async_config_t* config);

/**
 * @brief Create PAG bio-async bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
pag_bio_async_bridge_t* pag_bio_async_bridge_create(
    const pag_bio_async_config_t* config
);

/**
 * @brief Destroy PAG bio-async bridge
 *
 * @param bridge Bridge to destroy
 */
void pag_bio_async_bridge_destroy(pag_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to PAG instance and router
 *
 * @param bridge Bridge handle
 * @param pag PAG instance (opaque, cast internally)
 * @param router Bio-router for message dispatch
 * @return 0 on success, -1 on error
 */
int pag_bio_async_connect(
    pag_bio_async_bridge_t* bridge,
    void* pag,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int pag_bio_async_disconnect(pag_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool pag_bio_async_is_connected(const pag_bio_async_bridge_t* bridge);

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
int pag_bio_async_process_inbox(
    pag_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
int pag_bio_async_update(
    pag_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast defense state change
 *
 * @param bridge Bridge handle
 * @param defense_type Current defense response
 * @param intensity Response intensity [0, 1]
 * @param threat_level Current threat level
 * @return 0 on success, -1 on error
 */
int pag_bio_async_broadcast_defense(
    pag_bio_async_bridge_t* bridge,
    pag_bio_defense_type_t defense_type,
    float intensity,
    pag_bio_threat_level_t threat_level
);

/**
 * @brief Broadcast pain modulation signal
 *
 * @param bridge Bridge handle
 * @param analgesia_level Overall analgesia [0, 1]
 * @param descending_inhibition Signal to spinal cord [0, 1]
 * @param pain_intensity Input pain intensity [0, 1]
 * @return 0 on success, -1 on error
 */
int pag_bio_async_broadcast_pain_mod(
    pag_bio_async_bridge_t* bridge,
    float analgesia_level,
    float descending_inhibition,
    float pain_intensity
);

/**
 * @brief Broadcast emotional state
 *
 * @param bridge Bridge handle
 * @param emotion Dominant emotion
 * @param intensity Emotional intensity [0, 1]
 * @param valence Positive/negative [-1, +1]
 * @return 0 on success, -1 on error
 */
int pag_bio_async_broadcast_emotion(
    pag_bio_async_bridge_t* bridge,
    pag_bio_emotion_type_t emotion,
    float intensity,
    float valence
);

/**
 * @brief Broadcast vocalization trigger
 *
 * @param bridge Bridge handle
 * @param vocal_type Vocalization type
 * @param intensity Vocalization intensity [0, 1]
 * @param urgency Priority/urgency [0, 1]
 * @return 0 on success, -1 on error
 */
int pag_bio_async_broadcast_vocalization(
    pag_bio_async_bridge_t* bridge,
    pag_bio_vocal_type_t vocal_type,
    float intensity,
    float urgency
);

/**
 * @brief Broadcast autonomic response
 *
 * @param bridge Bridge handle
 * @param heart_rate_mod HR change [-1=brady, +1=tachy]
 * @param respiratory_mod RR change [-1, +1]
 * @param muscle_tone Muscle tone [0, 1]
 * @return 0 on success, -1 on error
 */
int pag_bio_async_broadcast_autonomic(
    pag_bio_async_bridge_t* bridge,
    float heart_rate_mod,
    float respiratory_mod,
    float muscle_tone
);

/**
 * @brief Broadcast threat level assessment
 *
 * @param bridge Bridge handle
 * @param level Threat proximity level
 * @param intensity Threat intensity [0, 1]
 * @param recommended Recommended defense response
 * @return 0 on success, -1 on error
 */
int pag_bio_async_broadcast_threat(
    pag_bio_async_bridge_t* bridge,
    pag_bio_threat_level_t level,
    float intensity,
    pag_bio_defense_type_t recommended
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to PAG messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (use PAG_BIO_BRIDGE_SUB_* macros)
 * @return 0 on success, -1 on error
 */
int pag_bio_async_subscribe_module(
    pag_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from PAG messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int pag_bio_async_unsubscribe_module(
    pag_bio_async_bridge_t* bridge,
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
int pag_bio_async_update_subscription(
    pag_bio_async_bridge_t* bridge,
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
uint32_t pag_bio_async_get_subscriber_count(
    const pag_bio_async_bridge_t* bridge,
    pag_bio_bridge_msg_type_t msg_type
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
int pag_bio_async_get_stats(
    const pag_bio_async_bridge_t* bridge,
    pag_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int pag_bio_async_reset_stats(pag_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Static string name
 */
const char* pag_bio_msg_type_name(pag_bio_bridge_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void pag_bio_async_print_summary(const pag_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PAG_BIO_ASYNC_BRIDGE_H */
