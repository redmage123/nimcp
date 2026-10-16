/**
 * @file nimcp_gust_bio_async_bridge.h
 * @brief Gustatory Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Central bio-async integration for gustatory cortex that provides
 *       comprehensive message routing for taste detection, flavor perception,
 *       food reward, disgust responses, and satiety modulation via the bio-router.
 *
 * WHY: The gustatory cortex requires system-wide communication for:
 *      - Route taste detection to hypothalamus for appetite modulation
 *      - Broadcast disgust signals to amygdala for aversive responses
 *      - Send reward signals to dopaminergic system
 *      - Integrate with olfactory for flavor perception
 *      - Signal toxic warnings for protective responses
 *
 * HOW: Registers gustatory as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes incoming
 *      satiety modulation and attention signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * GUSTATORY OUTPUT PATHWAYS:
 * --------------------------
 * 1. Hypothalamus (appetite):
 *    - Feeding behavior regulation
 *    - Satiety signaling
 *    - Mapped to: GUST_BIO_MSG_REWARD_SIGNAL, GUST_BIO_MSG_SATIETY_UPDATE
 *
 * 2. Amygdala (emotional):
 *    - Disgust responses
 *    - Conditioned taste aversion
 *    - Mapped to: GUST_BIO_MSG_DISGUST_ALERT, GUST_BIO_MSG_TOXIC_WARNING
 *
 * 3. Orbitofrontal Cortex:
 *    - Flavor perception (taste + smell)
 *    - Value computation
 *    - Mapped to: GUST_BIO_MSG_FLAVOR_RESULT, GUST_BIO_MSG_HEDONIC_VALUE
 *
 * 4. Nucleus Accumbens (reward):
 *    - Food reward signaling
 *    - Learned preferences
 *    - Mapped to: GUST_BIO_MSG_REWARD_SIGNAL, GUST_BIO_MSG_PREFERENCE_UPDATE
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GUST_BIO_ASYNC_BRIDGE_H
#define NIMCP_GUST_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for gustatory in bio-async system (0x3300 - 0x33FF reserved) */
#define BIO_MODULE_ID_GUSTATORY         0x3300

/** Maximum number of module subscriptions */
#define GUST_BIO_MAX_SUBSCRIPTIONS      64

/** Maximum pending messages in inbox/outbox */
#define GUST_BIO_MAX_INBOX_SIZE         256
#define GUST_BIO_MAX_OUTBOX_SIZE        128

/** Default broadcast interval (ms) */
#define GUST_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100

/** Message expiry time (ms) */
#define GUST_BIO_MESSAGE_TTL_MS         5000

/** Disgust alert threshold */
#define GUST_BIO_DISGUST_THRESHOLD      0.6f

/** Toxic warning intensity threshold */
#define GUST_BIO_TOXIC_THRESHOLD        0.3f

/** Reward signal threshold */
#define GUST_BIO_REWARD_THRESHOLD       0.4f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Gustatory bio-async message types
 */
typedef enum {
    GUST_BIO_MSG_TASTE_DETECTED = 0,    /**< New taste detected */
    GUST_BIO_MSG_FLAVOR_RESULT,         /**< Flavor perception result */
    GUST_BIO_MSG_REWARD_SIGNAL,         /**< Food reward signal */
    GUST_BIO_MSG_DISGUST_ALERT,         /**< Disgust/rejection alert */
    GUST_BIO_MSG_SATIETY_UPDATE,        /**< Satiety modulation */
    GUST_BIO_MSG_TOXIC_WARNING,         /**< Toxin detected */
    GUST_BIO_MSG_PREFERENCE_UPDATE,     /**< Preference change */
    GUST_BIO_MSG_HEDONIC_VALUE,         /**< Taste pleasantness */
    GUST_BIO_MSG_PALATABILITY,          /**< Palatability assessment */
    GUST_BIO_MSG_FOOD_CATEGORY,         /**< Food category identification */
    GUST_BIO_MSG_ADAPTATION,            /**< Taste adaptation state */
    GUST_BIO_MSG_REQUEST_STATE,         /**< Request current state */
    GUST_BIO_MSG_MODULATE_ATTENTION,    /**< Attention modulation request */
    GUST_BIO_MSG_COUNT
} gust_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define GUST_BIO_SUB_TASTE_DETECTED     (1U << GUST_BIO_MSG_TASTE_DETECTED)
#define GUST_BIO_SUB_FLAVOR_RESULT      (1U << GUST_BIO_MSG_FLAVOR_RESULT)
#define GUST_BIO_SUB_REWARD_SIGNAL      (1U << GUST_BIO_MSG_REWARD_SIGNAL)
#define GUST_BIO_SUB_DISGUST_ALERT      (1U << GUST_BIO_MSG_DISGUST_ALERT)
#define GUST_BIO_SUB_SATIETY_UPDATE     (1U << GUST_BIO_MSG_SATIETY_UPDATE)
#define GUST_BIO_SUB_TOXIC_WARNING      (1U << GUST_BIO_MSG_TOXIC_WARNING)
#define GUST_BIO_SUB_PREFERENCE_UPDATE  (1U << GUST_BIO_MSG_PREFERENCE_UPDATE)
#define GUST_BIO_SUB_HEDONIC_VALUE      (1U << GUST_BIO_MSG_HEDONIC_VALUE)
#define GUST_BIO_SUB_PALATABILITY       (1U << GUST_BIO_MSG_PALATABILITY)
#define GUST_BIO_SUB_FOOD_CATEGORY      (1U << GUST_BIO_MSG_FOOD_CATEGORY)
#define GUST_BIO_SUB_ADAPTATION         (1U << GUST_BIO_MSG_ADAPTATION)
#define GUST_BIO_SUB_ALL                (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Taste detected message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Taste profile */
    float sweet;                        /**< Sweet intensity [0, 1] */
    float salty;                        /**< Salty intensity [0, 1] */
    float sour;                         /**< Sour intensity [0, 1] */
    float bitter;                       /**< Bitter intensity [0, 1] */
    float umami;                        /**< Umami intensity [0, 1] */

    /* Additional properties */
    float overall_intensity;            /**< Overall taste intensity [0, 1] */
    float temperature;                  /**< Stimulus temperature (C) */
    float texture;                      /**< Texture smoothness [0, 1] */

    /* Processing state */
    bool is_novel;                      /**< Novel (unfamiliar) taste */
    uint32_t taste_id;                  /**< Internal taste tracking ID */

    uint64_t timestamp_us;              /**< Detection timestamp */
} gust_bio_taste_detected_msg_t;

/**
 * @brief Flavor result message payload (taste + smell integration)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Flavor identity */
    char flavor_name[64];               /**< Identified flavor name */
    food_category_t category;           /**< Food category */
    float identification_confidence;    /**< Identification confidence [0, 1] */

    /* Flavor complexity */
    float complexity;                   /**< Flavor complexity [0, 1] */
    float harmony;                      /**< Taste-smell harmony [0, 1] */

    /* Context */
    uint32_t taste_id;                  /**< Taste tracking ID */
    bool olfactory_integrated;          /**< Olfactory data integrated */

    uint64_t timestamp_us;              /**< Result timestamp */
} gust_bio_flavor_result_msg_t;

/**
 * @brief Reward signal message payload (to dopamine system)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Reward properties */
    float reward_magnitude;             /**< Reward magnitude [0, 1] */
    float novelty_bonus;                /**< Novelty contribution [0, 1] */
    float nutritional_value;            /**< Estimated nutritional value [0, 1] */

    /* Modulation */
    float satiety_modulation;           /**< Satiety-adjusted reward [0, 1] */
    float learned_preference;           /**< Learned preference contribution */

    /* Context */
    food_category_t food_category;      /**< Food category */
    uint32_t taste_id;                  /**< Taste tracking ID */

    uint64_t timestamp_us;              /**< Signal timestamp */
} gust_bio_reward_signal_msg_t;

/**
 * @brief Disgust alert message payload (to amygdala)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Disgust assessment */
    disgust_level_t level;              /**< Disgust severity */
    float intensity;                    /**< Disgust intensity [0, 1] */
    float urgency;                      /**< Response urgency [0, 1] */

    /* Cause identification */
    bool bitter_triggered;              /**< Bitter taste triggered */
    bool spoiled_detected;              /**< Spoilage detected */
    bool texture_aversion;              /**< Texture-related aversion */

    /* Behavioral trigger */
    bool triggers_gag;                  /**< May trigger gag reflex */
    bool triggers_rejection;            /**< Triggers rejection behavior */

    uint32_t taste_id;                  /**< Taste tracking ID */
    uint64_t timestamp_us;              /**< Alert timestamp */
} gust_bio_disgust_alert_msg_t;

/**
 * @brief Satiety update message payload (bidirectional with hypothalamus)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Satiety state */
    float satiety_level;                /**< Current satiety [0, 1] */
    float hunger_level;                 /**< Current hunger [0, 1] */
    float reward_modulation;            /**< How satiety modulates reward */

    /* Specific satieties */
    float sweet_satiety;                /**< Sweet-specific satiety [0, 1] */
    float salt_satiety;                 /**< Salt-specific satiety [0, 1] */

    /* Direction */
    bool is_request;                    /**< Request (true) or update (false) */

    uint64_t timestamp_us;              /**< Update timestamp */
} gust_bio_satiety_update_msg_t;

/**
 * @brief Toxic warning message payload (urgent)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Toxicity assessment */
    float toxicity_estimate;            /**< Estimated toxicity [0, 1] */
    float confidence;                   /**< Detection confidence [0, 1] */
    float urgency;                      /**< Response urgency [0, 1] */

    /* Toxic indicators */
    bool extreme_bitter;                /**< Extreme bitterness detected */
    bool chemical_taste;                /**< Chemical taste detected */
    bool spoilage_markers;              /**< Spoilage taste markers */

    /* Behavioral response */
    bool recommend_spit;                /**< Recommend spitting out */
    bool recommend_vomit;               /**< Recommend emetic response */

    uint32_t taste_id;                  /**< Taste tracking ID */
    uint64_t timestamp_us;              /**< Warning timestamp */
} gust_bio_toxic_warning_msg_t;

/**
 * @brief Preference update message payload (learning)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Preference changes */
    basic_taste_t taste_type;           /**< Which taste type */
    float old_preference;               /**< Previous preference [-1, 1] */
    float new_preference;               /**< New preference [-1, 1] */
    float preference_delta;             /**< Change amount */

    /* Learning context */
    bool positive_outcome;              /**< Positive (true) or negative (false) */
    float learning_rate;                /**< Applied learning rate */

    uint64_t timestamp_us;              /**< Update timestamp */
} gust_bio_preference_update_msg_t;

/**
 * @brief Hedonic value message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Hedonic assessment */
    taste_hedonic_t valence;            /**< Hedonic category */
    float hedonic_score;                /**< Hedonic score [-1, 1] */
    float arousal;                      /**< Arousal level [0, 1] */

    /* Contributing factors */
    float taste_contribution;           /**< Taste contribution to hedonic */
    float texture_contribution;         /**< Texture contribution */
    float temperature_contribution;     /**< Temperature contribution */

    uint32_t taste_id;                  /**< Taste tracking ID */
    uint64_t timestamp_us;              /**< Assessment timestamp */
} gust_bio_hedonic_value_msg_t;

/**
 * @brief Palatability message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Palatability assessment */
    float palatability;                 /**< Overall palatability [0, 1] */
    float taste_quality;                /**< Taste quality score [0, 1] */
    float expected_satisfaction;        /**< Expected satisfaction [0, 1] */

    /* Context */
    food_category_t category;           /**< Food category */
    bool matches_expectation;           /**< Matches expected taste */

    uint32_t taste_id;                  /**< Taste tracking ID */
    uint64_t timestamp_us;              /**< Assessment timestamp */
} gust_bio_palatability_msg_t;

/**
 * @brief Adaptation state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Adaptation state per taste */
    float sweet_adaptation;             /**< Sweet adaptation [0, 1] */
    float salty_adaptation;             /**< Salty adaptation [0, 1] */
    float sour_adaptation;              /**< Sour adaptation [0, 1] */
    float bitter_adaptation;            /**< Bitter adaptation [0, 1] */
    float umami_adaptation;             /**< Umami adaptation [0, 1] */

    /* Overall state */
    float overall_adaptation;           /**< Overall adaptation [0, 1] */
    bool fully_adapted;                 /**< Fully adapted (no taste) */

    uint64_t timestamp_us;              /**< State timestamp */
} gust_bio_adaptation_msg_t;

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
} gust_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Gustatory bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t taste_broadcast_interval_ms;    /**< Taste state broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Threshold settings */
    float disgust_threshold;                 /**< Disgust alert threshold */
    float toxic_threshold;                   /**< Toxic warning threshold */
    float reward_threshold;                  /**< Reward signal threshold */

    /* Priority settings */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t urgent_channel;  /**< Urgent channel */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum subscriptions */

    /* Feature flags */
    bool enable_reward_signals;              /**< Enable reward routing */
    bool enable_disgust_alerts;              /**< Enable disgust alerts */
    bool enable_toxic_warnings;              /**< Enable toxic warnings */
    bool enable_satiety_integration;         /**< Enable satiety integration */
    bool enable_preference_learning;         /**< Enable preference updates */
    bool enable_logging;                     /**< Enable logging */
} gust_bio_async_config_t;

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
    uint64_t tastes_detected;
    uint64_t flavors_identified;
    uint64_t rewards_sent;
    uint64_t disgust_alerts_sent;
    uint64_t toxic_warnings_sent;
    uint64_t satiety_updates_sent;
    uint64_t preference_updates_sent;
    uint64_t hedonic_sent;

    /* Subscription stats */
    uint32_t active_subscriptions;
    uint32_t peak_subscriptions;

    /* Timing stats */
    uint64_t last_broadcast_time_us;
    float avg_message_latency_us;

    /* Errors */
    uint64_t handler_errors;
    uint64_t routing_errors;
} gust_bio_async_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

typedef struct gust_bio_async_bridge_struct gust_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int gust_bio_async_default_config(gust_bio_async_config_t* config);
gust_bio_async_bridge_t* gust_bio_async_bridge_create(const gust_bio_async_config_t* config);
void gust_bio_async_bridge_destroy(gust_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int gust_bio_async_connect(gust_bio_async_bridge_t* bridge, nimcp_gustatory_t* gust, bio_router_t router);
int gust_bio_async_disconnect(gust_bio_async_bridge_t* bridge);
bool gust_bio_async_is_connected(const gust_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int gust_bio_async_process_inbox(gust_bio_async_bridge_t* bridge, uint32_t max_messages);
int gust_bio_async_update(gust_bio_async_bridge_t* bridge, uint32_t delta_ms);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

int gust_bio_async_broadcast_taste_detected(gust_bio_async_bridge_t* bridge, const taste_stimulus_t* stimulus, bool is_novel);
int gust_bio_async_broadcast_flavor_result(gust_bio_async_bridge_t* bridge, const char* name, food_category_t category, float confidence);
int gust_bio_async_broadcast_reward(gust_bio_async_bridge_t* bridge, float magnitude, food_category_t category);
int gust_bio_async_broadcast_disgust(gust_bio_async_bridge_t* bridge, disgust_level_t level, float intensity);
int gust_bio_async_broadcast_satiety(gust_bio_async_bridge_t* bridge, float satiety, float hunger);
int gust_bio_async_broadcast_toxic_warning(gust_bio_async_bridge_t* bridge, float toxicity, bool recommend_spit);
int gust_bio_async_broadcast_preference_update(gust_bio_async_bridge_t* bridge, basic_taste_t taste, float old_pref, float new_pref);
int gust_bio_async_broadcast_hedonic(gust_bio_async_bridge_t* bridge, taste_hedonic_t valence, float score);
int gust_bio_async_broadcast_palatability(gust_bio_async_bridge_t* bridge, float palatability, food_category_t category);
int gust_bio_async_broadcast_adaptation(gust_bio_async_bridge_t* bridge, const float* adaptation_levels, bool fully_adapted);

/* ============================================================================
 * Subscription API
 * ============================================================================ */

int gust_bio_async_subscribe_module(gust_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
int gust_bio_async_unsubscribe_module(gust_bio_async_bridge_t* bridge, uint32_t module_id);
uint32_t gust_bio_async_get_subscriber_count(const gust_bio_async_bridge_t* bridge, gust_bio_msg_type_t type);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int gust_bio_async_get_stats(const gust_bio_async_bridge_t* bridge, gust_bio_async_stats_t* stats);
int gust_bio_async_reset_stats(gust_bio_async_bridge_t* bridge);
const char* gust_bio_msg_type_name(gust_bio_msg_type_t msg_type);
void gust_bio_async_print_summary(const gust_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GUST_BIO_ASYNC_BRIDGE_H */
