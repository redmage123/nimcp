/**
 * @file nimcp_olfact_bio_async_bridge.h
 * @brief Olfactory Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Central bio-async integration for olfactory cortex that provides
 *       comprehensive message routing for odor detection, identification,
 *       emotional associations, and memory triggers via the bio-router.
 *
 * WHY: The olfactory cortex has unique direct connections to limbic system:
 *      - Route odor detection directly to amygdala (no thalamic relay)
 *      - Broadcast hedonic (pleasant/unpleasant) signals for behavior
 *      - Trigger episodic memories via entorhinal/hippocampal pathway
 *      - Signal food-related odors to hypothalamus for appetite modulation
 *
 * HOW: Registers olfactory as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes incoming
 *      attention modulation and expectation signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * OLFACTORY OUTPUT PATHWAYS:
 * --------------------------
 * 1. Amygdala (direct - unique to olfaction):
 *    - Emotional valence of odors
 *    - Fear-related odor associations
 *    - Mapped to: OLFACT_BIO_MSG_HEDONIC_VALUE, OLFACT_BIO_MSG_DANGER_ODOR
 *
 * 2. Entorhinal/Hippocampus (memory):
 *    - Episodic memory encoding
 *    - Proustian memory triggers
 *    - Mapped to: OLFACT_BIO_MSG_MEMORY_TRIGGER
 *
 * 3. Orbitofrontal Cortex:
 *    - Odor identification and naming
 *    - Value computation
 *    - Mapped to: OLFACT_BIO_MSG_ODOR_IDENTIFIED
 *
 * 4. Hypothalamus:
 *    - Appetite regulation
 *    - Food detection
 *    - Mapped to: OLFACT_BIO_MSG_FOOD_SIGNAL
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OLFACT_BIO_ASYNC_BRIDGE_H
#define NIMCP_OLFACT_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/olfactory/nimcp_olfactory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for olfactory in bio-async system (0x3200 - 0x32FF reserved) */
#define BIO_MODULE_ID_OLFACTORY         0x3200

/** Maximum number of module subscriptions */
#define OLFACT_BIO_MAX_SUBSCRIPTIONS    64

/** Maximum pending messages in inbox/outbox */
#define OLFACT_BIO_MAX_INBOX_SIZE       256
#define OLFACT_BIO_MAX_OUTBOX_SIZE      128

/** Default broadcast interval (ms) */
#define OLFACT_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100

/** Message expiry time (ms) */
#define OLFACT_BIO_MESSAGE_TTL_MS       5000

/** Danger odor intensity threshold */
#define OLFACT_BIO_DANGER_THRESHOLD     0.8f

/** Food odor salience threshold */
#define OLFACT_BIO_FOOD_THRESHOLD       0.5f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Olfactory bio-async message types
 */
typedef enum {
    OLFACT_BIO_MSG_ODOR_DETECTED = 0,   /**< New odor detected */
    OLFACT_BIO_MSG_ODOR_IDENTIFIED,     /**< Odor identification result */
    OLFACT_BIO_MSG_HEDONIC_VALUE,       /**< Pleasant/unpleasant signal */
    OLFACT_BIO_MSG_MEMORY_TRIGGER,      /**< Olfactory memory activated */
    OLFACT_BIO_MSG_SNIFF_CYCLE,         /**< Sniff phase update */
    OLFACT_BIO_MSG_FOOD_SIGNAL,         /**< Food-related odor */
    OLFACT_BIO_MSG_DANGER_ODOR,         /**< Dangerous odor (smoke, etc.) */
    OLFACT_BIO_MSG_ADAPTATION,          /**< Adaptation state change */
    OLFACT_BIO_MSG_CONCENTRATION,       /**< Odor concentration change */
    OLFACT_BIO_MSG_FAMILIARITY,         /**< Familiarity assessment */
    OLFACT_BIO_MSG_REQUEST_STATE,       /**< Request current state */
    OLFACT_BIO_MSG_MODULATE_ATTENTION,  /**< Attention modulation request */
    OLFACT_BIO_MSG_COUNT
} olfact_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define OLFACT_BIO_SUB_ODOR_DETECTED    (1U << OLFACT_BIO_MSG_ODOR_DETECTED)
#define OLFACT_BIO_SUB_ODOR_IDENTIFIED  (1U << OLFACT_BIO_MSG_ODOR_IDENTIFIED)
#define OLFACT_BIO_SUB_HEDONIC_VALUE    (1U << OLFACT_BIO_MSG_HEDONIC_VALUE)
#define OLFACT_BIO_SUB_MEMORY_TRIGGER   (1U << OLFACT_BIO_MSG_MEMORY_TRIGGER)
#define OLFACT_BIO_SUB_SNIFF_CYCLE      (1U << OLFACT_BIO_MSG_SNIFF_CYCLE)
#define OLFACT_BIO_SUB_FOOD_SIGNAL      (1U << OLFACT_BIO_MSG_FOOD_SIGNAL)
#define OLFACT_BIO_SUB_DANGER_ODOR      (1U << OLFACT_BIO_MSG_DANGER_ODOR)
#define OLFACT_BIO_SUB_ADAPTATION       (1U << OLFACT_BIO_MSG_ADAPTATION)
#define OLFACT_BIO_SUB_CONCENTRATION    (1U << OLFACT_BIO_MSG_CONCENTRATION)
#define OLFACT_BIO_SUB_FAMILIARITY      (1U << OLFACT_BIO_MSG_FAMILIARITY)
#define OLFACT_BIO_SUB_ALL              (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Odor detected message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Odor properties */
    float intensity;                    /**< Odor intensity [0, 1] */
    float concentration;                /**< Concentration estimate */
    uint32_t receptor_pattern[16];      /**< Receptor activation pattern */

    /* Processing state */
    bool is_novel;                      /**< Novel (unfamiliar) odor */
    bool is_salient;                    /**< Salient (attention-grabbing) */
    uint32_t odor_id;                   /**< Internal odor tracking ID */

    uint64_t timestamp_us;              /**< Detection timestamp */
} olfact_bio_odor_detected_msg_t;

/**
 * @brief Odor identified message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Identification results */
    char odor_name[64];                 /**< Identified odor name */
    odor_category_t category;           /**< Odor category */
    float confidence;                   /**< Identification confidence [0, 1] */
    float familiarity;                  /**< Familiarity score [0, 1] */

    /* Context */
    uint32_t odor_id;                   /**< Odor tracking ID */
    uint64_t timestamp_us;              /**< Identification timestamp */
} olfact_bio_odor_identified_msg_t;

/**
 * @brief Hedonic value message payload (to amygdala)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Hedonic assessment */
    hedonic_valence_t valence;          /**< Pleasantness category */
    float hedonic_score;                /**< Hedonic score [-1, 1] */
    float arousal;                      /**< Arousal level [0, 1] */

    /* Emotional associations */
    bool triggers_fear;                 /**< Associated with fear */
    bool triggers_disgust;              /**< Associated with disgust */
    bool triggers_attraction;           /**< Associated with attraction */

    uint32_t odor_id;                   /**< Odor tracking ID */
    uint64_t timestamp_us;              /**< Assessment timestamp */
} olfact_bio_hedonic_value_msg_t;

/**
 * @brief Memory trigger message payload (to hippocampus)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Memory association */
    uint32_t memory_id;                 /**< Associated memory ID */
    float memory_strength;              /**< Strength of association [0, 1] */
    float emotional_intensity;          /**< Emotional intensity [0, 1] */

    /* Context */
    char context_hint[64];              /**< Context hint string */
    bool is_episodic;                   /**< Episodic vs semantic memory */

    uint32_t odor_id;                   /**< Triggering odor ID */
    uint64_t timestamp_us;              /**< Trigger timestamp */
} olfact_bio_memory_trigger_msg_t;

/**
 * @brief Food signal message payload (to hypothalamus)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Food properties */
    float food_salience;                /**< Food salience [0, 1] */
    float appetite_stimulation;         /**< Appetite stimulation [0, 1] */
    bool is_sweet;                      /**< Sweet-associated odor */
    bool is_savory;                     /**< Savory-associated odor */
    bool is_spoiled;                    /**< Spoilage detection */

    /* Nutritional hints */
    float caloric_estimate;             /**< Estimated caloric value */
    bool triggers_satiety;              /**< May trigger satiety */

    uint32_t odor_id;                   /**< Odor tracking ID */
    uint64_t timestamp_us;              /**< Signal timestamp */
} olfact_bio_food_signal_msg_t;

/**
 * @brief Danger odor message payload (urgent)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Danger assessment */
    uint32_t danger_type;               /**< Type of danger */
    float danger_intensity;             /**< Danger intensity [0, 1] */
    float urgency;                      /**< Response urgency [0, 1] */

    /* Danger categories */
    bool is_smoke;                      /**< Smoke detected */
    bool is_gas;                        /**< Gas leak detected */
    bool is_chemical;                   /**< Chemical hazard */
    bool is_decay;                      /**< Biological decay */

    uint32_t odor_id;                   /**< Odor tracking ID */
    uint64_t timestamp_us;              /**< Detection timestamp */
} olfact_bio_danger_odor_msg_t;

/**
 * @brief Sniff cycle message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Sniff state */
    sniff_phase_t phase;                /**< Current sniff phase */
    float sniff_strength;               /**< Sniff strength [0, 1] */
    float airflow_rate;                 /**< Airflow rate estimate */

    /* Timing */
    uint32_t cycle_count;               /**< Total sniff cycles */
    float cycle_frequency_hz;           /**< Sniff frequency */

    uint64_t timestamp_us;              /**< Phase timestamp */
} olfact_bio_sniff_cycle_msg_t;

/**
 * @brief Adaptation state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Adaptation state */
    float adaptation_level;             /**< Current adaptation [0, 1] */
    float time_constant_ms;             /**< Adaptation time constant */
    bool fully_adapted;                 /**< Fully adapted (no perception) */

    uint32_t odor_id;                   /**< Adapted odor ID */
    uint64_t timestamp_us;              /**< State timestamp */
} olfact_bio_adaptation_msg_t;

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
} olfact_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Olfactory bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t odor_broadcast_interval_ms;     /**< Odor state broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float danger_odor_threshold;             /**< Danger odor threshold */
    float food_salience_threshold;           /**< Food salience threshold */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t urgent_channel;  /**< Urgent channel */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum subscriptions */

    /* Feature flags */
    bool enable_memory_triggers;             /**< Enable memory routing */
    bool enable_food_signals;                /**< Enable food signaling */
    bool enable_danger_alerts;               /**< Enable danger alerts */
    bool enable_hedonic_routing;             /**< Enable hedonic routing */
    bool enable_logging;                     /**< Enable logging */
} olfact_bio_async_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t broadcasts_sent;

    /* Per-type counts */
    uint64_t odors_detected;
    uint64_t odors_identified;
    uint64_t hedonic_sent;
    uint64_t memory_triggers_sent;
    uint64_t food_signals_sent;
    uint64_t danger_alerts_sent;

    /* Subscription stats */
    uint32_t active_subscriptions;
    uint32_t peak_subscriptions;

    /* Timing stats */
    uint64_t last_broadcast_time_us;
    float avg_message_latency_us;

    /* Errors */
    uint64_t handler_errors;
    uint64_t routing_errors;
} olfact_bio_async_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

typedef struct olfact_bio_async_bridge_struct olfact_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int olfact_bio_async_default_config(olfact_bio_async_config_t* config);
olfact_bio_async_bridge_t* olfact_bio_async_bridge_create(const olfact_bio_async_config_t* config);
void olfact_bio_async_bridge_destroy(olfact_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int olfact_bio_async_connect(olfact_bio_async_bridge_t* bridge, nimcp_olfactory_t* olfact, bio_router_t router);
int olfact_bio_async_disconnect(olfact_bio_async_bridge_t* bridge);
bool olfact_bio_async_is_connected(const olfact_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int olfact_bio_async_process_inbox(olfact_bio_async_bridge_t* bridge, uint32_t max_messages);
int olfact_bio_async_update(olfact_bio_async_bridge_t* bridge, uint32_t delta_ms);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int olfact_bio_async_broadcast_odor_detected(olfact_bio_async_bridge_t* bridge, float intensity, bool is_novel);
int olfact_bio_async_broadcast_odor_identified(olfact_bio_async_bridge_t* bridge, const char* name, odor_category_t category, float confidence);
int olfact_bio_async_broadcast_hedonic(olfact_bio_async_bridge_t* bridge, hedonic_valence_t valence, float score);
int olfact_bio_async_broadcast_memory_trigger(olfact_bio_async_bridge_t* bridge, uint32_t memory_id, float strength);
int olfact_bio_async_broadcast_food_signal(olfact_bio_async_bridge_t* bridge, float salience, bool is_sweet, bool is_savory);
int olfact_bio_async_broadcast_danger_odor(olfact_bio_async_bridge_t* bridge, uint32_t danger_type, float intensity);
int olfact_bio_async_broadcast_sniff_cycle(olfact_bio_async_bridge_t* bridge, sniff_phase_t phase, float strength);
int olfact_bio_async_broadcast_adaptation(olfact_bio_async_bridge_t* bridge, float level, bool fully_adapted);

/* ============================================================================
 * Subscription API
 * ============================================================================ */

int olfact_bio_async_subscribe_module(olfact_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
int olfact_bio_async_unsubscribe_module(olfact_bio_async_bridge_t* bridge, uint32_t module_id);
uint32_t olfact_bio_async_get_subscriber_count(const olfact_bio_async_bridge_t* bridge, olfact_bio_msg_type_t type);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int olfact_bio_async_get_stats(const olfact_bio_async_bridge_t* bridge, olfact_bio_async_stats_t* stats);
int olfact_bio_async_reset_stats(olfact_bio_async_bridge_t* bridge);
const char* olfact_bio_msg_type_name(olfact_bio_msg_type_t msg_type);
void olfact_bio_async_print_summary(const olfact_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLFACT_BIO_ASYNC_BRIDGE_H */
