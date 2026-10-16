/**
 * @file nimcp_hypothalamus_bio_async_bridge.h
 * @brief Unified Hypothalamus Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Central bio-async integration for hypothalamus that provides comprehensive
 *       message routing between the hypothalamus and all NIMCP modules via the bio-router.
 *
 * WHY: The hypothalamus is the brain's central homeostatic regulator and must communicate
 *      with virtually every brain system. This bridge enables:
 *      - Event-driven drive state broadcasts to cognitive systems
 *      - Reception of homeostatic signals from peripheral modules
 *      - Coordination of circadian phase information system-wide
 *      - Routing of stress/arousal signals for system-wide modulation
 *
 * HOW: Registers hypothalamus as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming homeostatic requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * HYPOTHALAMIC OUTPUT PATHWAYS:
 * -----------------------------
 * 1. Humoral Output (Pituitary → Hormones):
 *    - CRH → ACTH → Cortisol (stress response)
 *    - TRH → TSH → Thyroid (metabolism)
 *    - GnRH → FSH/LH (reproduction)
 *    - GHRH → GH (growth)
 *    - Mapped to: Bio-async broadcast messages
 *
 * 2. Neural Output (Direct projections):
 *    - Lateral hypothalamus → Arousal systems
 *    - Paraventricular → Autonomic centers
 *    - Suprachiasmatic → Circadian timing signals
 *    - Mapped to: Targeted bio-async messages
 *
 * 3. Neuromodulatory Output:
 *    - Orexin/hypocretin → Arousal (lateral hypothalamus)
 *    - Histamine → Alertness (tuberomammillary nucleus)
 *    - Mapped to: Neuromodulator channel selection
 *
 * HYPOTHALAMIC INPUT PATHWAYS:
 * ----------------------------
 * 1. Interoceptive (body state):
 *    - Vagus nerve → Visceral state
 *    - Blood-borne signals → Energy state
 *    - Mapped to: Homeostatic alert messages
 *
 * 2. Limbic (emotional):
 *    - Amygdala → Threat signals
 *    - Hippocampus → Contextual modulation
 *    - Mapped to: Stress/arousal messages
 *
 * 3. Cortical (cognitive):
 *    - Prefrontal → Goal-directed modulation
 *    - Insula → Interoceptive awareness
 *    - Mapped to: Drive modulation requests
 *
 * MESSAGE ROUTING ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║                 HYPOTHALAMUS BIO-ASYNC BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║                                                                                ║
 * ║   OUTBOUND (Hypothalamus → Modules)                                           ║
 * ║   ─────────────────────────────────                                           ║
 * ║   ┌─────────────────┐     ┌──────────────────────────────────────────────┐   ║
 * ║   │  Drive State    │────▶│ BIO_ROUTER: Broadcast to all subscribers     │   ║
 * ║   │  Circadian      │     │  - Attention, Executive, Memory, Emotion     │   ║
 * ║   │  Stress/Arousal │     │  - Global Workspace, Wellbeing, Ethics       │   ║
 * ║   │  Reward Signals │     │  - Salience, Curiosity, Introspection        │   ║
 * ║   └─────────────────┘     └──────────────────────────────────────────────┘   ║
 * ║                                                                                ║
 * ║   INBOUND (Modules → Hypothalamus)                                            ║
 * ║   ─────────────────────────────────                                           ║
 * ║   ┌──────────────────────────────────────────────┐     ┌─────────────────┐   ║
 * ║   │ BIO_ROUTER: Receive from registered modules  │────▶│ Homeostatic     │   ║
 * ║   │  - Drive state requests                      │     │ Processing      │   ║
 * ║   │  - Drive modulation commands                 │     │ & Response      │   ║
 * ║   │  - Peripheral homeostatic signals            │     │ Generation      │   ║
 * ║   └──────────────────────────────────────────────┘     └─────────────────┘   ║
 * ║                                                                                ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
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

#ifndef NIMCP_HYPOTHALAMUS_BIO_ASYNC_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
/* Note: We use hypo_drive_state_t from orchestrator.h, not drives.h */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Drive Type Definitions (compatible with drives.h)
 * ============================================================================ */

/**
 * @brief Drive type enumeration for bio-async messages
 *
 * WHAT: Identifies specific drive types in message payloads
 * WHY:  Allows receivers to interpret drive-specific messages
 * HOW:  Enum values match nimcp_hypothalamus_drives.h
 *
 * NOTE: This is defined here to avoid circular dependencies with drives.h
 *       which also defines hypo_drive_state_t (conflicting with orchestrator.h)
 */
#ifndef HYPO_DRIVE_TYPE_DEFINED
#define HYPO_DRIVE_TYPE_DEFINED
typedef enum {
    /* Physiological drives (survival) */
    HYPO_DRIVE_HUNGER = 0,          /**< Food-seeking motivation */
    HYPO_DRIVE_THIRST,              /**< Water-seeking motivation */
    HYPO_DRIVE_TEMPERATURE,         /**< Thermoregulatory motivation */
    HYPO_DRIVE_FATIGUE,             /**< Rest-seeking motivation */

    /* Psychological drives (growth) */
    HYPO_DRIVE_SOCIAL,              /**< Social connection motivation */
    HYPO_DRIVE_CURIOSITY,           /**< Information-seeking motivation */
    HYPO_DRIVE_SAFETY,              /**< Threat avoidance motivation */
    HYPO_DRIVE_AUTONOMY,            /**< Self-determination motivation */
    HYPO_DRIVE_COMPETENCE,          /**< Mastery-seeking motivation */

    HYPO_DRIVE_COUNT
} hypo_drive_type_t;
#endif /* HYPO_DRIVE_TYPE_DEFINED */

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define HYPO_BIO_MAX_SUBSCRIPTIONS      64

/** Maximum pending messages in inbox */
#define HYPO_BIO_MAX_INBOX_SIZE         256

/** Maximum pending messages in outbox */
#define HYPO_BIO_MAX_OUTBOX_SIZE        128

/** Default broadcast interval for drive state (ms) */
#define HYPO_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100

/** Message expiry time (ms) */
#define HYPO_BIO_MESSAGE_TTL_MS         5000

/** Urgent drive threshold for priority messaging */
#define HYPO_BIO_URGENT_DRIVE_THRESHOLD 0.85f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Hypothalamus bio-async message types
 *
 * WHAT: Message type enumeration for hypothalamus bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific hypothalamic output pathway
 */
typedef enum {
    HYPO_BIO_MSG_DRIVE_STATE = 0,       /**< Complete drive state broadcast */
    HYPO_BIO_MSG_DRIVE_URGENT,          /**< Single urgent drive notification */
    HYPO_BIO_MSG_HOMEOSTATIC_ALERT,     /**< Homeostatic deviation alert */
    HYPO_BIO_MSG_CIRCADIAN_PHASE,       /**< Circadian phase update */
    HYPO_BIO_MSG_STRESS_LEVEL,          /**< Current stress/cortisol level */
    HYPO_BIO_MSG_AROUSAL_STATE,         /**< Arousal/alertness level */
    HYPO_BIO_MSG_AUTONOMIC_STATE,       /**< Sympathetic/parasympathetic balance */
    HYPO_BIO_MSG_REWARD_SIGNAL,         /**< Reward signal for learning */
    HYPO_BIO_MSG_TEMPERATURE,           /**< Core temperature */
    HYPO_BIO_MSG_FATIGUE_LEVEL,         /**< Fatigue/sleep pressure */
    HYPO_BIO_MSG_REQUEST_DRIVE,         /**< Request current drive state */
    HYPO_BIO_MSG_MODULATE_DRIVE,        /**< External drive modulation request */
    HYPO_BIO_MSG_COUNT
} hypo_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define HYPO_BIO_SUB_DRIVE_STATE        (1U << HYPO_BIO_MSG_DRIVE_STATE)
#define HYPO_BIO_SUB_DRIVE_URGENT       (1U << HYPO_BIO_MSG_DRIVE_URGENT)
#define HYPO_BIO_SUB_HOMEOSTATIC_ALERT  (1U << HYPO_BIO_MSG_HOMEOSTATIC_ALERT)
#define HYPO_BIO_SUB_CIRCADIAN_PHASE    (1U << HYPO_BIO_MSG_CIRCADIAN_PHASE)
#define HYPO_BIO_SUB_STRESS_LEVEL       (1U << HYPO_BIO_MSG_STRESS_LEVEL)
#define HYPO_BIO_SUB_AROUSAL_STATE      (1U << HYPO_BIO_MSG_AROUSAL_STATE)
#define HYPO_BIO_SUB_AUTONOMIC_STATE    (1U << HYPO_BIO_MSG_AUTONOMIC_STATE)
#define HYPO_BIO_SUB_REWARD_SIGNAL      (1U << HYPO_BIO_MSG_REWARD_SIGNAL)
#define HYPO_BIO_SUB_TEMPERATURE        (1U << HYPO_BIO_MSG_TEMPERATURE)
#define HYPO_BIO_SUB_FATIGUE_LEVEL      (1U << HYPO_BIO_MSG_FATIGUE_LEVEL)
#define HYPO_BIO_SUB_ALL                (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Drive state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Drive levels [0, 1] for each drive type */
    float drive_levels[HYPO_DRIVE_COUNT];
    float drive_urgencies[HYPO_DRIVE_COUNT];

    /* Summary information */
    hypo_drive_type_t primary_drive;    /**< Highest priority drive */
    float primary_urgency;              /**< Primary drive urgency */
    float unified_drive_level;          /**< Combined drive intensity */

    /* Timing */
    uint64_t timestamp_us;              /**< Measurement timestamp */
} hypo_bio_drive_state_msg_t;

/**
 * @brief Urgent drive notification payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    hypo_drive_type_t drive_type;       /**< Which drive is urgent */
    float drive_level;                  /**< Current level [0, 1] */
    float urgency;                      /**< Urgency weight [0, 1] */
    float deviation_from_setpoint;      /**< How far from optimal */

    uint64_t timestamp_us;              /**< When urgency detected */
} hypo_bio_drive_urgent_msg_t;

/**
 * @brief Homeostatic alert payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t variable_id;               /**< Which homeostatic variable */
    float current_value;                /**< Current measured value */
    float setpoint;                     /**< Target setpoint */
    float deviation;                    /**< Signed deviation */
    float severity;                     /**< Alert severity [0, 1] */
    bool is_critical;                   /**< Requires immediate action */

    uint64_t timestamp_us;              /**< Alert timestamp */
} hypo_bio_homeostatic_alert_msg_t;

/**
 * @brief Circadian phase message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float phase;                        /**< Current phase [0, 2*PI] */
    float phase_normalized;             /**< Phase as [0, 1] (0=midnight) */
    float alertness_factor;             /**< Circadian alertness [0, 1] */
    float sleep_propensity;             /**< Sleep drive from circadian */

    uint32_t time_of_day_hours;         /**< Hours since midnight */
    uint32_t time_of_day_minutes;       /**< Minutes past hour */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hypo_bio_circadian_msg_t;

/**
 * @brief Stress level message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float stress_level;                 /**< Overall stress [0, 1] */
    float cortisol_level;               /**< HPA axis output [0, 1] */
    float crh_level;                    /**< CRH release level [0, 1] */

    bool is_acute;                      /**< Acute vs chronic */
    bool hpa_axis_active;               /**< HPA axis activated */

    uint32_t stressor_source;           /**< What triggered stress */
    uint64_t stress_onset_us;           /**< When stress began */
    uint64_t timestamp_us;              /**< Current timestamp */
} hypo_bio_stress_msg_t;

/**
 * @brief Arousal state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float arousal_level;                /**< Overall arousal [0, 1] */
    float alertness;                    /**< Alertness level [0, 1] */
    float orexin_level;                 /**< Orexin/hypocretin [0, 1] */
    float histamine_level;              /**< TMN histamine [0, 1] */

    bool is_sleep_deprived;             /**< Sleep deprivation flag */
    float vigilance;                    /**< Sustained attention capacity */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hypo_bio_arousal_msg_t;

/**
 * @brief Autonomic state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float sympathetic_tone;             /**< Sympathetic activity [0, 1] */
    float parasympathetic_tone;         /**< Parasympathetic activity [0, 1] */
    float autonomic_balance;            /**< Balance (-1=para, +1=sympa) */

    float heart_rate_mod;               /**< Heart rate modulation factor */
    float respiratory_mod;              /**< Respiratory modulation factor */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hypo_bio_autonomic_msg_t;

/**
 * @brief Reward signal message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float reward_magnitude;             /**< Reward signal strength [-1, 1] */
    float prediction_error;             /**< RPE for learning */

    hypo_drive_type_t source_drive;     /**< Which drive generated reward */
    bool is_intrinsic;                  /**< Intrinsic vs extrinsic */

    float dopamine_release;             /**< Resulting DA release [0, 1] */
    uint32_t target_module;             /**< Target for directed reward */

    uint64_t timestamp_us;              /**< Reward timestamp */
} hypo_bio_reward_msg_t;

/**
 * @brief Temperature message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float core_temperature;             /**< Core temp (Celsius) */
    float setpoint_temperature;         /**< Target temp (Celsius) */
    float deviation;                    /**< Deviation from setpoint */

    bool is_fever;                      /**< Fever state active */
    bool needs_cooling;                 /**< Cooling response needed */
    bool needs_heating;                 /**< Heating response needed */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hypo_bio_temperature_msg_t;

/**
 * @brief Fatigue level message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float fatigue_level;                /**< Overall fatigue [0, 1] */
    float sleep_pressure;               /**< Homeostatic sleep drive [0, 1] */
    float adenosine_level;              /**< Adenosine accumulation [0, 1] */

    float time_awake_hours;             /**< Hours since last sleep */
    bool sleep_deprived;                /**< Significant deprivation */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} hypo_bio_fatigue_msg_t;

/**
 * @brief Drive modulation request payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    hypo_drive_type_t drive_type;       /**< Which drive to modulate */
    float modulation_amount;            /**< Amount to add/subtract */
    bool is_relative;                   /**< Relative vs absolute */

    uint32_t requester_module;          /**< Who is requesting */
    uint32_t request_reason;            /**< Why modulation requested */

    uint64_t timestamp_us;              /**< Request timestamp */
} hypo_bio_modulate_request_msg_t;

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
} hypo_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Hypothalamus bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t drive_broadcast_interval_ms;   /**< Drive state broadcast interval */
    uint32_t circadian_broadcast_interval_ms; /**< Circadian broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast drive state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;  /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float urgent_drive_threshold;            /**< Threshold for urgent messages */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
    nimcp_bio_channel_type_t urgent_channel;  /**< Channel for urgent messages */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_reward_routing;              /**< Enable reward signal routing */
    bool enable_stress_routing;              /**< Enable stress signal routing */
    bool enable_circadian_routing;           /**< Enable circadian routing */
    bool enable_autonomic_routing;           /**< Enable autonomic routing */
    bool enable_logging;                     /**< Enable message logging */
} hypo_bio_async_config_t;

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
    uint64_t drive_state_broadcasts;         /**< Drive state broadcasts */
    uint64_t urgent_notifications;           /**< Urgent drive notifications */
    uint64_t circadian_broadcasts;           /**< Circadian phase broadcasts */
    uint64_t stress_broadcasts;              /**< Stress level broadcasts */
    uint64_t reward_signals_sent;            /**< Reward signals routed */

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
} hypo_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus bio-async bridge handle
 */
typedef struct hypo_bio_async_bridge_struct hypo_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for bio-async bridge configuration
 * WHY:  Easy initialization with biologically-realistic parameters
 * HOW:  Return struct with evidence-based timing and thresholds
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_default_config(hypo_bio_async_config_t* config);

/**
 * @brief Create hypothalamus bio-async bridge
 *
 * WHAT: Initialize bio-async integration for hypothalamus
 * WHY:  Enable message routing between hypothalamus and all modules
 * HOW:  Allocate structure, initialize subscription registry, prepare handlers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
hypo_bio_async_bridge_t* hypo_bio_async_bridge_create(
    const hypo_bio_async_config_t* config
);

/**
 * @brief Destroy hypothalamus bio-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect from router, free subscriptions, release memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void hypo_bio_async_bridge_destroy(hypo_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to orchestrator and router
 *
 * WHAT: Establish connections to hypothalamus orchestrator and bio-router
 * WHY:  Enable bidirectional message flow
 * HOW:  Register with router, link to orchestrator, set up handlers
 *
 * @param bridge Bio-async bridge
 * @param orch Hypothalamus orchestrator
 * @param router Bio-router (NULL to use global)
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_connect(
    hypo_bio_async_bridge_t* bridge,
    hypo_orchestrator_t orch,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Disconnect from bio-router
 * WHY:  Clean disconnection before shutdown
 * HOW:  Unregister handlers, clear module context
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_disconnect(hypo_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bio-async bridge
 * @return true if connected to both orchestrator and router
 */
bool hypo_bio_async_is_connected(const hypo_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Process pending messages from bio-router inbox
 * WHY:  Handle incoming drive requests and modulation commands
 * HOW:  Pop messages, dispatch to appropriate handlers, update state
 *
 * @param bridge Bio-async bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int hypo_bio_async_process_inbox(
    hypo_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Perform periodic bridge updates
 * WHY:  Send scheduled broadcasts, expire old messages
 * HOW:  Check timers, send due broadcasts, cleanup expired entries
 *
 * @param bridge Bio-async bridge
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_update(
    hypo_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast complete drive state to all subscribers
 *
 * WHAT: Send current drive state to all subscribed modules
 * WHY:  System-wide drive state awareness for coordinated behavior
 * HOW:  Gather drive state from orchestrator, format message, broadcast
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_drive_state(hypo_bio_async_bridge_t* bridge);

/**
 * @brief Broadcast circadian phase update
 *
 * WHAT: Send current circadian phase to subscribers
 * WHY:  Enable circadian modulation of cognitive processes
 * HOW:  Package phase information, broadcast to circadian subscribers
 *
 * @param bridge Bio-async bridge
 * @param phase Circadian phase [0, 2*PI]
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_circadian(
    hypo_bio_async_bridge_t* bridge,
    float phase
);

/**
 * @brief Broadcast stress level update
 *
 * WHAT: Send current stress/cortisol level to subscribers
 * WHY:  Enable stress modulation of cognitive and physiological processes
 * HOW:  Package stress state, broadcast on high-priority channel
 *
 * @param bridge Bio-async bridge
 * @param cortisol Cortisol level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_stress(
    hypo_bio_async_bridge_t* bridge,
    float cortisol
);

/**
 * @brief Broadcast arousal state update
 *
 * WHAT: Send current arousal/alertness to subscribers
 * WHY:  Enable arousal-modulated attention and processing
 * HOW:  Package arousal state, broadcast to subscribers
 *
 * @param bridge Bio-async bridge
 * @param arousal Arousal level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_arousal(
    hypo_bio_async_bridge_t* bridge,
    float arousal
);

/**
 * @brief Broadcast autonomic state update
 *
 * WHAT: Send sympathetic/parasympathetic balance to subscribers
 * WHY:  Enable autonomic-aware processing
 * HOW:  Package autonomic state, broadcast to subscribers
 *
 * @param bridge Bio-async bridge
 * @param sympathetic Sympathetic tone [0, 1]
 * @param parasympathetic Parasympathetic tone [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_autonomic(
    hypo_bio_async_bridge_t* bridge,
    float sympathetic,
    float parasympathetic
);

/**
 * @brief Broadcast homeostatic alert
 *
 * WHAT: Send homeostatic deviation alert to system
 * WHY:  Trigger system-wide response to homeostatic imbalance
 * HOW:  Package alert, broadcast on high-priority channel
 *
 * @param bridge Bio-async bridge
 * @param variable_id Homeostatic variable ID
 * @param current_value Current value
 * @param setpoint Target setpoint
 * @param is_critical Critical flag
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_homeostatic_alert(
    hypo_bio_async_bridge_t* bridge,
    uint32_t variable_id,
    float current_value,
    float setpoint,
    bool is_critical
);

/**
 * @brief Send urgent drive notification
 *
 * WHAT: Send high-priority notification about urgent drive
 * WHY:  Immediate attention to critical drives (e.g., safety threat)
 * HOW:  Use NOREPINEPHRINE channel for priority routing
 *
 * @param bridge Bio-async bridge
 * @param drive_type Which drive is urgent
 * @param drive_level Current drive level
 * @param urgency Urgency weight
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_send_urgent_drive(
    hypo_bio_async_bridge_t* bridge,
    hypo_drive_type_t drive_type,
    float drive_level,
    float urgency
);

/**
 * @brief Send reward signal to specific module
 *
 * WHAT: Send reward/punishment signal for learning
 * WHY:  Enable reward-based learning in target modules
 * HOW:  Use DOPAMINE channel, target specific module
 *
 * @param bridge Bio-async bridge
 * @param reward Reward magnitude [-1, +1]
 * @param target_module Target module ID (0 = broadcast)
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_send_reward(
    hypo_bio_async_bridge_t* bridge,
    float reward,
    uint32_t target_module
);

/**
 * @brief Broadcast temperature state
 *
 * WHAT: Send core temperature information
 * WHY:  Enable temperature-aware processing
 * HOW:  Package temperature data, broadcast to subscribers
 *
 * @param bridge Bio-async bridge
 * @param core_temp Core temperature (Celsius)
 * @param setpoint_temp Setpoint temperature (Celsius)
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_temperature(
    hypo_bio_async_bridge_t* bridge,
    float core_temp,
    float setpoint_temp
);

/**
 * @brief Broadcast fatigue level
 *
 * WHAT: Send fatigue/sleep pressure information
 * WHY:  Enable fatigue-aware resource allocation
 * HOW:  Package fatigue data, broadcast to subscribers
 *
 * @param bridge Bio-async bridge
 * @param fatigue Fatigue level [0, 1]
 * @param sleep_pressure Sleep pressure [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_broadcast_fatigue(
    hypo_bio_async_bridge_t* bridge,
    float fatigue,
    float sleep_pressure
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to hypothalamus messages
 *
 * WHAT: Register module to receive specific message types
 * WHY:  Enable selective message routing to interested modules
 * HOW:  Add entry to subscription registry with type mask
 *
 * @param bridge Bio-async bridge
 * @param module_id Module requesting subscription
 * @param msg_types Bitmask of message types (HYPO_BIO_SUB_*)
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_subscribe_module(
    hypo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from hypothalamus messages
 *
 * WHAT: Remove module subscription
 * WHY:  Clean unsubscription when module no longer needs messages
 * HOW:  Remove entry from subscription registry
 *
 * @param bridge Bio-async bridge
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_unsubscribe_module(
    hypo_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 *
 * WHAT: Modify message types for existing subscription
 * WHY:  Allow dynamic subscription changes
 * HOW:  Update type mask in subscription registry
 *
 * @param bridge Bio-async bridge
 * @param module_id Module to update
 * @param msg_types New bitmask of message types
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_update_subscription(
    hypo_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * @param bridge Bio-async bridge
 * @param msg_type Message type to query
 * @return Number of subscribers for this type
 */
uint32_t hypo_bio_async_get_subscriber_count(
    const hypo_bio_async_bridge_t* bridge,
    hypo_bio_msg_type_t msg_type
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
int hypo_bio_async_get_stats(
    const hypo_bio_async_bridge_t* bridge,
    hypo_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_reset_stats(hypo_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name string
 */
const char* hypo_bio_msg_type_name(hypo_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bio-async bridge (NULL-safe)
 */
void hypo_bio_async_print_summary(const hypo_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_BIO_ASYNC_BRIDGE_H */
