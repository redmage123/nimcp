/**
 * @file nimcp_perception_bio_async_bridge.h
 * @brief Perception Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for perception systems providing
 *       comprehensive message routing for visual, auditory, somatosensory,
 *       olfactory, and gustatory modalities via the bio-router.
 *
 * WHY: The perception system must coordinate across multiple sensory modalities:
 *      - Route feature detections to attention and cognitive systems
 *      - Broadcast object recognition results to memory and motor systems
 *      - Synchronize multi-sensory binding for unified perception
 *      - Coordinate cross-modal attention shifts
 *      - Enable predictive coding across sensory hierarchies
 *
 * HOW: Registers perception as a bio-router module, maintains per-modality
 *      subscription registries, provides typed message broadcast APIs, and
 *      processes incoming attention modulation and top-down prediction requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * PERCEPTION OUTPUT PATHWAYS:
 * ---------------------------
 * 1. Feature detection broadcasts:
 *    - Visual features (edges, colors, motion, faces, objects)
 *    - Auditory features (tones, phonemes, words, spatial sources)
 *    - Somatosensory features (touch, pain, proprioception, temperature)
 *    - Chemical senses (olfactory molecules, gustatory tastes)
 *
 * 2. Object/Scene recognition:
 *    - Ventral "what" pathway: Object identity
 *    - Dorsal "where" pathway: Spatial location and motion
 *    - Multi-sensory binding: Cross-modal object coherence
 *
 * 3. Attention and salience:
 *    - Bottom-up salience signals to attention system
 *    - Cross-modal attention coordination
 *    - Novelty and surprise detection
 *
 * PERCEPTION INPUT PATHWAYS:
 * --------------------------
 * 1. Top-down attention modulation:
 *    - Feature gain modulation from prefrontal cortex
 *    - Expectation-based enhancement from hippocampus
 *
 * 2. Predictive coding:
 *    - Predictions from higher cortical areas
 *    - Precision weighting for prediction errors
 *
 * 3. Cross-modal coordination:
 *    - Binding requests from multi-sensory areas
 *    - Temporal synchronization signals
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

#ifndef NIMCP_PERCEPTION_BIO_ASYNC_BRIDGE_H
#define NIMCP_PERCEPTION_BIO_ASYNC_BRIDGE_H

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
 * Error Codes
 * ============================================================================ */

/** Perception bio-async error base */
#define PERCEPT_BIO_ERROR_BASE              11000
#define PERCEPT_BIO_ERROR_NOT_INITIALIZED   (PERCEPT_BIO_ERROR_BASE + 1)
#define PERCEPT_BIO_ERROR_INVALID_MODALITY  (PERCEPT_BIO_ERROR_BASE + 2)
#define PERCEPT_BIO_ERROR_BINDING_FAILED    (PERCEPT_BIO_ERROR_BASE + 3)
#define PERCEPT_BIO_ERROR_NO_SUBSCRIBERS    (PERCEPT_BIO_ERROR_BASE + 4)
#define PERCEPT_BIO_ERROR_QUEUE_FULL        (PERCEPT_BIO_ERROR_BASE + 5)
#define PERCEPT_BIO_ERROR_TIMEOUT           (PERCEPT_BIO_ERROR_BASE + 6)
#define PERCEPT_BIO_ERROR_INVALID_FEATURE   (PERCEPT_BIO_ERROR_BASE + 7)
#define PERCEPT_BIO_ERROR_CROSS_MODAL_FAIL  (PERCEPT_BIO_ERROR_BASE + 8)

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for perception in bio-async system (0x3200 - 0x32FF reserved) */
#define BIO_MODULE_ID_PERCEPTION            0x3200
#define BIO_MODULE_ID_PERCEPTION_VISUAL     0x3210
#define BIO_MODULE_ID_PERCEPTION_AUDITORY   0x3220
#define BIO_MODULE_ID_PERCEPTION_SOMATO     0x3230
#define BIO_MODULE_ID_PERCEPTION_OLFACTORY  0x3240
#define BIO_MODULE_ID_PERCEPTION_GUSTATORY  0x3250
#define BIO_MODULE_ID_PERCEPTION_BINDING    0x3260

/** Maximum number of module subscriptions per modality */
#define PERCEPT_BIO_MAX_SUBSCRIPTIONS       64

/** Maximum pending messages in inbox */
#define PERCEPT_BIO_MAX_INBOX_SIZE          512

/** Maximum pending messages in outbox */
#define PERCEPT_BIO_MAX_OUTBOX_SIZE         256

/** Default broadcast interval for perceptual state (ms) */
#define PERCEPT_BIO_DEFAULT_BROADCAST_INTERVAL_MS   33  /* ~30 Hz */

/** Message expiry time (ms) */
#define PERCEPT_BIO_MESSAGE_TTL_MS          5000

/** Maximum features per broadcast */
#define PERCEPT_BIO_MAX_FEATURES_PER_MSG    32

/** Maximum modalities for cross-modal binding */
#define PERCEPT_BIO_MAX_MODALITIES          5

/** Salience threshold for urgent messaging */
#define PERCEPT_BIO_SALIENCE_URGENT_THRESHOLD 0.8f

/* ============================================================================
 * Enumerations - Message Types
 * ============================================================================ */

/**
 * @brief Perception bio-async message types
 *
 * WHAT: Message type enumeration for perception bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific perceptual output pathway
 */
typedef enum {
    /* Visual perception messages (0x00 - 0x1F) */
    PERCEPT_MSG_VISUAL_FEATURE = 0,         /**< Visual feature detected */
    PERCEPT_MSG_VISUAL_EDGE,                /**< Edge detection result */
    PERCEPT_MSG_VISUAL_COLOR,               /**< Color detection result */
    PERCEPT_MSG_VISUAL_MOTION,              /**< Motion detection result */
    PERCEPT_MSG_VISUAL_FACE,                /**< Face detected */
    PERCEPT_MSG_VISUAL_OBJECT,              /**< Object recognized */
    PERCEPT_MSG_VISUAL_SCENE,               /**< Scene categorized */
    PERCEPT_MSG_VISUAL_DEPTH,               /**< Depth estimation */
    PERCEPT_MSG_VISUAL_ATTENTION_SHIFT,     /**< Visual attention shift */
    PERCEPT_MSG_VISUAL_SACCADE,             /**< Saccade event */

    /* Auditory perception messages (0x20 - 0x3F) */
    PERCEPT_MSG_AUDITORY_FEATURE = 0x20,    /**< Auditory feature detected */
    PERCEPT_MSG_AUDITORY_TONE,              /**< Pure tone detected */
    PERCEPT_MSG_AUDITORY_ONSET,             /**< Sound onset detected */
    PERCEPT_MSG_AUDITORY_OFFSET,            /**< Sound offset detected */
    PERCEPT_MSG_AUDITORY_PHONEME,           /**< Phoneme recognized */
    PERCEPT_MSG_AUDITORY_WORD,              /**< Word recognized */
    PERCEPT_MSG_AUDITORY_SPEAKER,           /**< Speaker identified */
    PERCEPT_MSG_AUDITORY_SPATIAL,           /**< Spatial localization */
    PERCEPT_MSG_AUDITORY_ATTENTION_SHIFT,   /**< Auditory attention shift */
    PERCEPT_MSG_AUDITORY_STREAM,            /**< Auditory stream segregation */

    /* Somatosensory perception messages (0x40 - 0x5F) */
    PERCEPT_MSG_SOMATO_FEATURE = 0x40,      /**< Somatosensory feature */
    PERCEPT_MSG_SOMATO_TOUCH,               /**< Touch event */
    PERCEPT_MSG_SOMATO_PRESSURE,            /**< Pressure sensing */
    PERCEPT_MSG_SOMATO_VIBRATION,           /**< Vibration sensing */
    PERCEPT_MSG_SOMATO_TEXTURE,             /**< Texture recognition */
    PERCEPT_MSG_SOMATO_PAIN,                /**< Pain signal */
    PERCEPT_MSG_SOMATO_TEMPERATURE,         /**< Temperature sensing */
    PERCEPT_MSG_SOMATO_PROPRIOCEPTION,      /**< Proprioceptive update */
    PERCEPT_MSG_SOMATO_BODY_POSITION,       /**< Body position estimate */

    /* Olfactory perception messages (0x60 - 0x6F) */
    PERCEPT_MSG_OLFACTORY_FEATURE = 0x60,   /**< Olfactory feature */
    PERCEPT_MSG_OLFACTORY_ODOR,             /**< Odor detected */
    PERCEPT_MSG_OLFACTORY_INTENSITY,        /**< Odor intensity */
    PERCEPT_MSG_OLFACTORY_CATEGORY,         /**< Odor category */
    PERCEPT_MSG_OLFACTORY_FAMILIAR,         /**< Familiar odor recognition */

    /* Gustatory perception messages (0x70 - 0x7F) */
    PERCEPT_MSG_GUSTATORY_FEATURE = 0x70,   /**< Gustatory feature */
    PERCEPT_MSG_GUSTATORY_TASTE,            /**< Taste detected */
    PERCEPT_MSG_GUSTATORY_INTENSITY,        /**< Taste intensity */
    PERCEPT_MSG_GUSTATORY_CATEGORY,         /**< Taste category (sweet, sour, etc.) */
    PERCEPT_MSG_GUSTATORY_PALATABILITY,     /**< Palatability assessment */

    /* Cross-modal and binding messages (0x80 - 0x9F) */
    PERCEPT_MSG_BINDING_REQUEST = 0x80,     /**< Request cross-modal binding */
    PERCEPT_MSG_BINDING_RESULT,             /**< Binding result */
    PERCEPT_MSG_BINDING_CONFLICT,           /**< Binding conflict detected */
    PERCEPT_MSG_MULTIMODAL_OBJECT,          /**< Multi-modal object percept */
    PERCEPT_MSG_MULTIMODAL_SYNC,            /**< Multi-modal synchronization */
    PERCEPT_MSG_CROSS_MODAL_ATTENTION,      /**< Cross-modal attention shift */
    PERCEPT_MSG_TEMPORAL_BINDING,           /**< Temporal binding window */

    /* Object recognition messages (0xA0 - 0xAF) */
    PERCEPT_MSG_OBJECT_DETECTED = 0xA0,     /**< Object detected in scene */
    PERCEPT_MSG_OBJECT_IDENTIFIED,          /**< Object identity confirmed */
    PERCEPT_MSG_OBJECT_TRACKED,             /**< Object tracking update */
    PERCEPT_MSG_OBJECT_LOST,                /**< Object tracking lost */
    PERCEPT_MSG_OBJECT_CATEGORY,            /**< Object category assigned */
    PERCEPT_MSG_OBJECT_AFFORDANCE,          /**< Object affordance detected */

    /* Salience and attention messages (0xB0 - 0xBF) */
    PERCEPT_MSG_SALIENCE_MAP = 0xB0,        /**< Salience map update */
    PERCEPT_MSG_ATTENTION_TARGET,           /**< Attention target identified */
    PERCEPT_MSG_NOVELTY_DETECTED,           /**< Novel stimulus detected */
    PERCEPT_MSG_SURPRISE_SIGNAL,            /**< Surprise/prediction error */
    PERCEPT_MSG_HABITUATION,                /**< Stimulus habituation */

    /* Top-down modulation (incoming) (0xC0 - 0xCF) */
    PERCEPT_MSG_GAIN_MODULATION = 0xC0,     /**< Gain modulation request */
    PERCEPT_MSG_PREDICTION_UPDATE,          /**< Top-down prediction update */
    PERCEPT_MSG_ATTENTION_BIAS,             /**< Attention bias request */
    PERCEPT_MSG_EXPECTATION_SET,            /**< Set expectation for feature */

    /* System messages (0xF0 - 0xFF) */
    PERCEPT_MSG_STATE_QUERY = 0xF0,         /**< Query perception state */
    PERCEPT_MSG_STATE_RESPONSE,             /**< Perception state response */
    PERCEPT_MSG_CONFIG_UPDATE,              /**< Configuration update */
    PERCEPT_MSG_CALIBRATION,                /**< Sensor calibration event */
    PERCEPT_MSG_ERROR_REPORT,               /**< Error report */

    PERCEPT_MSG_COUNT = 0x100               /**< Total message type count */
} percept_bio_msg_type_t;

/* ============================================================================
 * Enumerations - Sensory Modalities
 * ============================================================================ */

/**
 * @brief Sensory modality enumeration
 */
typedef enum {
    PERCEPT_MODALITY_VISUAL = 0,            /**< Visual perception */
    PERCEPT_MODALITY_AUDITORY,              /**< Auditory perception */
    PERCEPT_MODALITY_SOMATOSENSORY,         /**< Somatosensory perception */
    PERCEPT_MODALITY_OLFACTORY,             /**< Olfactory perception */
    PERCEPT_MODALITY_GUSTATORY,             /**< Gustatory perception */
    PERCEPT_MODALITY_COUNT
} percept_modality_t;

/**
 * @brief Visual feature types
 */
typedef enum {
    VISUAL_FEATURE_EDGE = 0,
    VISUAL_FEATURE_COLOR,
    VISUAL_FEATURE_TEXTURE,
    VISUAL_FEATURE_MOTION,
    VISUAL_FEATURE_DEPTH,
    VISUAL_FEATURE_FACE,
    VISUAL_FEATURE_BODY,
    VISUAL_FEATURE_OBJECT,
    VISUAL_FEATURE_SCENE,
    VISUAL_FEATURE_COUNT
} visual_feature_type_t;

/**
 * @brief Auditory feature types
 */
typedef enum {
    AUDITORY_FEATURE_FREQUENCY = 0,
    AUDITORY_FEATURE_AMPLITUDE,
    AUDITORY_FEATURE_ONSET,
    AUDITORY_FEATURE_OFFSET,
    AUDITORY_FEATURE_PITCH,
    AUDITORY_FEATURE_TIMBRE,
    AUDITORY_FEATURE_SPATIAL,
    AUDITORY_FEATURE_PHONEME,
    AUDITORY_FEATURE_WORD,
    AUDITORY_FEATURE_COUNT
} auditory_feature_type_t;

/**
 * @brief Binding state
 */
typedef enum {
    BINDING_STATE_NONE = 0,                 /**< No binding */
    BINDING_STATE_TEMPORAL,                 /**< Temporally bound */
    BINDING_STATE_SPATIAL,                  /**< Spatially bound */
    BINDING_STATE_FULL,                     /**< Fully bound (coherent object) */
    BINDING_STATE_CONFLICT                  /**< Binding conflict */
} binding_state_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Standard bio-async message header for perception
 */
typedef struct {
    bio_message_header_t bio_header;        /**< Standard bio-async header */
    percept_bio_msg_type_t percept_type;    /**< Perception-specific type */
    percept_modality_t modality;            /**< Source modality */
    float salience;                         /**< Salience level [0, 1] */
    uint64_t timestamp_us;                  /**< Event timestamp */
} percept_message_header_t;

/**
 * @brief Visual feature message payload
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    visual_feature_type_t feature_type;     /**< Type of visual feature */
    float position[2];                      /**< Position in visual field [x, y] */
    float size[2];                          /**< Size [width, height] */
    float orientation;                      /**< Orientation (radians) */
    float strength;                         /**< Feature strength [0, 1] */

    /* Feature-specific data */
    union {
        struct {
            float direction;                /**< Edge direction */
            float contrast;                 /**< Edge contrast */
        } edge;
        struct {
            float rgb[3];                   /**< RGB color values */
            float saturation;               /**< Color saturation */
        } color;
        struct {
            float velocity[2];              /**< Motion velocity [vx, vy] */
            float coherence;                /**< Motion coherence */
        } motion;
        struct {
            float depth_m;                  /**< Depth in meters */
            float confidence;               /**< Depth confidence */
        } depth;
    } data;

    uint32_t feature_id;                    /**< Unique feature ID */
    uint32_t frame_id;                      /**< Source frame ID */
} percept_visual_feature_msg_t;

/**
 * @brief Object detection message payload
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    uint32_t object_id;                     /**< Unique object ID */
    uint32_t category_id;                   /**< Object category */
    char object_label[64];                  /**< Object label string */

    float position[3];                      /**< 3D position [x, y, z] */
    float bounding_box[4];                  /**< 2D bbox [x, y, w, h] */
    float confidence;                       /**< Detection confidence [0, 1] */

    /* Multi-modal binding */
    percept_modality_t primary_modality;    /**< Primary detection modality */
    uint32_t modality_mask;                 /**< Modalities contributing */
    binding_state_t binding_state;          /**< Cross-modal binding state */

    uint64_t first_seen_us;                 /**< When object first detected */
    uint64_t last_updated_us;               /**< Last update timestamp */
} percept_object_detected_msg_t;

/**
 * @brief Auditory feature message payload
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    auditory_feature_type_t feature_type;   /**< Type of auditory feature */
    float frequency_hz;                     /**< Center frequency */
    float amplitude;                        /**< Amplitude [0, 1] */
    float duration_ms;                      /**< Duration in ms */

    /* Spatial information */
    float azimuth;                          /**< Azimuth angle (radians) */
    float elevation;                        /**< Elevation angle (radians) */
    float distance;                         /**< Estimated distance */

    /* Feature-specific data */
    union {
        struct {
            float pitch_hz;                 /**< Detected pitch */
            float pitch_strength;           /**< Pitch detection strength */
        } pitch;
        struct {
            uint32_t phoneme_id;            /**< Phoneme ID */
            float phoneme_confidence;       /**< Recognition confidence */
        } phoneme;
        struct {
            uint32_t word_id;               /**< Word ID */
            char word_text[32];             /**< Recognized word text */
            float word_confidence;          /**< Recognition confidence */
        } word;
    } data;

    uint32_t feature_id;                    /**< Unique feature ID */
    uint32_t stream_id;                     /**< Auditory stream ID */
} percept_auditory_feature_msg_t;

/**
 * @brief Multi-modal binding request message
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    /* Binding request parameters */
    uint32_t binding_id;                    /**< Unique binding request ID */
    percept_modality_t modalities[PERCEPT_BIO_MAX_MODALITIES]; /**< Modalities to bind */
    uint32_t modality_count;                /**< Number of modalities */

    /* Temporal window */
    uint64_t window_start_us;               /**< Binding window start */
    uint64_t window_end_us;                 /**< Binding window end */

    /* Spatial constraints */
    float spatial_position[3];              /**< Expected position */
    float spatial_tolerance;                /**< Position tolerance */

    /* Feature IDs from each modality */
    uint32_t visual_feature_id;             /**< Visual feature ID (0 if none) */
    uint32_t auditory_feature_id;           /**< Auditory feature ID (0 if none) */
    uint32_t somato_feature_id;             /**< Somatosensory feature ID (0 if none) */
    uint32_t olfactory_feature_id;          /**< Olfactory feature ID (0 if none) */
    uint32_t gustatory_feature_id;          /**< Gustatory feature ID (0 if none) */

    uint64_t timestamp_us;                  /**< Request timestamp */
} percept_binding_request_msg_t;

/**
 * @brief Multi-modal binding result message
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    uint32_t binding_id;                    /**< Binding request ID */
    binding_state_t result;                 /**< Binding result */
    float binding_strength;                 /**< Binding strength [0, 1] */

    /* Bound object information */
    uint32_t bound_object_id;               /**< Resulting object ID */
    uint32_t modalities_bound;              /**< Bitmask of bound modalities */

    /* Conflict information (if any) */
    bool has_conflict;                      /**< Whether conflict detected */
    percept_modality_t conflict_modalities[2]; /**< Conflicting modalities */
    float conflict_severity;                /**< Conflict severity [0, 1] */

    uint64_t binding_time_us;               /**< Time to complete binding */
    uint64_t timestamp_us;                  /**< Result timestamp */
} percept_binding_result_msg_t;

/**
 * @brief Salience map update message
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    percept_modality_t modality;            /**< Source modality */
    uint32_t map_width;                     /**< Map width */
    uint32_t map_height;                    /**< Map height */

    /* Peak salience location */
    float peak_position[2];                 /**< Peak salience position [x, y] */
    float peak_salience;                    /**< Peak salience value */

    /* Top salient regions */
    uint32_t num_regions;                   /**< Number of salient regions */
    struct {
        float position[2];                  /**< Region center */
        float salience;                     /**< Region salience */
        float size;                         /**< Region size */
    } regions[8];                           /**< Up to 8 salient regions */

    uint64_t timestamp_us;                  /**< Map timestamp */
} percept_salience_map_msg_t;

/**
 * @brief Attention modulation request message (incoming)
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    percept_modality_t target_modality;     /**< Target modality */
    float gain_multiplier;                  /**< Gain multiplier [0.5, 2.0] */
    float position[3];                      /**< Spatial attention target */
    float spatial_extent;                   /**< Spatial extent of attention */

    /* Feature-based attention */
    bool feature_attention;                 /**< Enable feature attention */
    uint32_t feature_type;                  /**< Feature type to enhance */
    float feature_value;                    /**< Feature value to match */
    float feature_tolerance;                /**< Feature matching tolerance */

    uint32_t requester_module;              /**< Requesting module ID */
    uint64_t duration_ms;                   /**< Attention duration */
    uint64_t timestamp_us;                  /**< Request timestamp */
} percept_gain_modulation_msg_t;

/**
 * @brief Prediction update message (incoming top-down)
 */
typedef struct {
    percept_message_header_t header;        /**< Perception header */

    percept_modality_t modality;            /**< Target modality */
    uint32_t predicted_object_id;           /**< Predicted object ID */
    float predicted_position[3];            /**< Predicted position */
    float prediction_precision;             /**< Precision of prediction */

    /* Feature predictions */
    uint32_t num_features;                  /**< Number of predicted features */
    struct {
        uint32_t feature_type;              /**< Feature type */
        float predicted_value;              /**< Predicted value */
        float precision;                    /**< Feature precision */
    } features[8];                          /**< Up to 8 feature predictions */

    uint64_t timestamp_us;                  /**< Prediction timestamp */
} percept_prediction_update_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Per-modality subscription entry
 */
typedef struct {
    bio_module_id_t module_id;              /**< Subscribed module ID */
    uint32_t msg_type_mask_low;             /**< Low 32 bits of subscribed types */
    uint32_t msg_type_mask_high;            /**< High 32 bits of subscribed types */
    percept_modality_t modality;            /**< Subscribed modality */
    bool active;                            /**< Subscription active */
    uint64_t subscription_time;             /**< When subscribed */
    uint64_t messages_sent;                 /**< Messages sent to this sub */
} percept_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Perception bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t visual_broadcast_interval_ms;      /**< Visual broadcast interval */
    uint32_t auditory_broadcast_interval_ms;    /**< Auditory broadcast interval */
    uint32_t somato_broadcast_interval_ms;      /**< Somatosensory broadcast interval */
    uint32_t binding_broadcast_interval_ms;     /**< Binding result interval */
    bool enable_auto_broadcast;                 /**< Auto-broadcast perceptual state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;      /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                    /**< Message time-to-live */

    /* Priority settings */
    float salience_urgent_threshold;            /**< Threshold for urgent messages */
    nimcp_bio_channel_type_t default_channel;   /**< Default channel */
    nimcp_bio_channel_type_t feature_channel;   /**< Channel for feature messages */
    nimcp_bio_channel_type_t object_channel;    /**< Channel for object messages */
    nimcp_bio_channel_type_t urgent_channel;    /**< Channel for urgent messages */

    /* Subscription limits */
    uint32_t max_subscriptions_per_modality;    /**< Max subscriptions per modality */

    /* Feature flags - per modality */
    bool enable_visual_routing;                 /**< Enable visual perception routing */
    bool enable_auditory_routing;               /**< Enable auditory perception routing */
    bool enable_somato_routing;                 /**< Enable somatosensory routing */
    bool enable_olfactory_routing;              /**< Enable olfactory routing */
    bool enable_gustatory_routing;              /**< Enable gustatory routing */

    /* Cross-modal integration */
    bool enable_binding;                        /**< Enable cross-modal binding */
    bool enable_salience_integration;           /**< Enable integrated salience map */
    bool enable_prediction_error;               /**< Enable prediction error signals */

    /* Debugging */
    bool enable_logging;                        /**< Enable message logging */
} perception_bio_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Per-modality statistics
 */
typedef struct {
    uint64_t features_sent;                     /**< Features broadcast */
    uint64_t objects_detected;                  /**< Objects detected */
    uint64_t attention_shifts;                  /**< Attention shift events */
    uint64_t prediction_errors;                 /**< Prediction error signals */
    uint32_t active_subscriptions;              /**< Active subscriptions */
} percept_modality_stats_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Overall message counts */
    uint64_t messages_sent;                     /**< Total messages sent */
    uint64_t messages_received;                 /**< Total messages received */
    uint64_t messages_dropped;                  /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;                   /**< Broadcast messages sent */

    /* Per-modality statistics */
    percept_modality_stats_t modality_stats[PERCEPT_MODALITY_COUNT];

    /* Cross-modal statistics */
    uint64_t binding_requests;                  /**< Binding requests received */
    uint64_t bindings_successful;               /**< Successful bindings */
    uint64_t bindings_failed;                   /**< Failed bindings */
    uint64_t binding_conflicts;                 /**< Binding conflicts detected */

    /* Object tracking */
    uint64_t objects_tracked;                   /**< Total objects tracked */
    uint64_t objects_lost;                      /**< Objects lost */

    /* Timing stats */
    uint64_t last_broadcast_time_us;            /**< Last broadcast timestamp */
    float avg_message_latency_us;               /**< Average message latency */
    float max_message_latency_us;               /**< Peak message latency */
    float avg_binding_time_us;                  /**< Average binding time */

    /* Error counts */
    uint64_t handler_errors;                    /**< Message handler errors */
    uint64_t routing_errors;                    /**< Routing failures */
} perception_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Perception bio-async bridge handle
 */
typedef struct perception_bio_bridge_struct perception_bio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for perception bio-async integration
 * WHY:  Easy initialization with biologically-realistic parameters
 * HOW:  Returns configuration optimized for multi-sensory perception
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int perception_bio_async_default_config(perception_bio_bridge_config_t* config);

/**
 * @brief Create perception bio-async bridge
 *
 * WHAT: Initialize multi-modal perception bio-async integration
 * WHY:  Enable cross-modal perception coordination via messaging
 * HOW:  Allocate bridge, initialize per-modality subscription registries
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
perception_bio_bridge_t* perception_bio_async_bridge_create(
    const perception_bio_bridge_config_t* config
);

/**
 * @brief Destroy perception bio-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory and disconnect integrations
 * HOW:  Unregister from router, free state, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void perception_bio_async_bridge_destroy(perception_bio_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-router
 *
 * WHAT: Register perception module with bio-async router
 * WHY:  Enable message routing for perception events
 * HOW:  Register module contexts for each modality
 *
 * @param bridge Perception bridge
 * @param router Bio-router instance
 * @return 0 on success, error code on failure
 */
int perception_bio_async_connect(
    perception_bio_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Unregister perception from bio-async routing
 * WHY:  Clean shutdown of perception messaging
 * HOW:  Unregister all modality contexts
 *
 * @param bridge Perception bridge
 * @return 0 on success, error code on failure
 */
int perception_bio_async_disconnect(perception_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Perception bridge
 * @return true if connected to router
 */
bool perception_bio_async_is_connected(const perception_bio_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Process pending perception messages
 * WHY:  Handle attention modulation and prediction updates
 * HOW:  Invoke registered handlers for each message type
 *
 * @param bridge Perception bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, or -1 on error
 */
int perception_bio_async_process_inbox(
    perception_bio_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Periodic update of perception bridge state
 * WHY:  Trigger auto-broadcasts and maintenance
 * HOW:  Check broadcast intervals, update statistics
 *
 * @param bridge Perception bridge
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, error code on failure
 */
int perception_bio_async_update(
    perception_bio_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Visual Perception Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast visual feature detection
 *
 * WHAT: Send visual feature to all subscribers
 * WHY:  Notify downstream modules of feature detection
 * HOW:  Create typed message, route to subscribers
 *
 * @param bridge Perception bridge
 * @param feature_type Type of visual feature
 * @param position Position in visual field [x, y]
 * @param strength Feature strength [0, 1]
 * @param feature_data Feature-specific data (type-dependent)
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_visual_feature(
    perception_bio_bridge_t* bridge,
    visual_feature_type_t feature_type,
    const float* position,
    float strength,
    const void* feature_data
);

/**
 * @brief Broadcast visual object detection
 *
 * WHAT: Send object detection to all subscribers
 * WHY:  Notify downstream modules of recognized object
 * HOW:  Create object message, route on object channel
 *
 * @param bridge Perception bridge
 * @param object_id Unique object ID
 * @param category_id Object category
 * @param label Object label string
 * @param position 3D position [x, y, z]
 * @param confidence Detection confidence [0, 1]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_visual_object(
    perception_bio_bridge_t* bridge,
    uint32_t object_id,
    uint32_t category_id,
    const char* label,
    const float* position,
    float confidence
);

/**
 * @brief Broadcast visual attention shift
 *
 * WHAT: Send attention shift event
 * WHY:  Coordinate visual attention across modules
 * HOW:  High-priority broadcast on attention channel
 *
 * @param bridge Perception bridge
 * @param new_target New attention target position [x, y]
 * @param salience Salience at new target
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_visual_attention(
    perception_bio_bridge_t* bridge,
    const float* new_target,
    float salience
);

/* ============================================================================
 * Auditory Perception Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast auditory feature detection
 *
 * @param bridge Perception bridge
 * @param feature_type Type of auditory feature
 * @param frequency_hz Center frequency
 * @param amplitude Amplitude [0, 1]
 * @param duration_ms Duration in milliseconds
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_auditory_feature(
    perception_bio_bridge_t* bridge,
    auditory_feature_type_t feature_type,
    float frequency_hz,
    float amplitude,
    float duration_ms
);

/**
 * @brief Broadcast word recognition
 *
 * @param bridge Perception bridge
 * @param word_text Recognized word text
 * @param confidence Recognition confidence [0, 1]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_word_recognized(
    perception_bio_bridge_t* bridge,
    const char* word_text,
    float confidence
);

/**
 * @brief Broadcast auditory spatial localization
 *
 * @param bridge Perception bridge
 * @param azimuth Azimuth angle (radians)
 * @param elevation Elevation angle (radians)
 * @param distance Estimated distance
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_auditory_spatial(
    perception_bio_bridge_t* bridge,
    float azimuth,
    float elevation,
    float distance
);

/* ============================================================================
 * Somatosensory Perception Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast somatosensory touch event
 *
 * @param bridge Perception bridge
 * @param body_part Body part identifier
 * @param position Position on body part [x, y, z]
 * @param pressure Pressure level [0, 1]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_touch(
    perception_bio_bridge_t* bridge,
    uint32_t body_part,
    const float* position,
    float pressure
);

/**
 * @brief Broadcast pain signal
 *
 * @param bridge Perception bridge
 * @param body_part Body part identifier
 * @param intensity Pain intensity [0, 1]
 * @param is_urgent Whether pain requires immediate response
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_pain(
    perception_bio_bridge_t* bridge,
    uint32_t body_part,
    float intensity,
    bool is_urgent
);

/**
 * @brief Broadcast proprioceptive update
 *
 * @param bridge Perception bridge
 * @param body_part Body part identifier
 * @param position Position estimate [x, y, z]
 * @param velocity Velocity estimate [vx, vy, vz]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_proprioception(
    perception_bio_bridge_t* bridge,
    uint32_t body_part,
    const float* position,
    const float* velocity
);

/* ============================================================================
 * Cross-Modal Integration API
 * ============================================================================ */

/**
 * @brief Request cross-modal feature binding
 *
 * WHAT: Request binding of features across modalities
 * WHY:  Enable multi-sensory object perception
 * HOW:  Send binding request with feature IDs and constraints
 *
 * @param bridge Perception bridge
 * @param modalities Array of modalities to bind
 * @param modality_count Number of modalities
 * @param feature_ids Feature IDs for each modality
 * @param temporal_window_ms Temporal binding window
 * @return Binding request ID, or 0 on failure
 */
uint32_t perception_bio_async_request_binding(
    perception_bio_bridge_t* bridge,
    const percept_modality_t* modalities,
    uint32_t modality_count,
    const uint32_t* feature_ids,
    uint32_t temporal_window_ms
);

/**
 * @brief Broadcast binding result
 *
 * WHAT: Send result of binding operation
 * WHY:  Notify subscribers of successful/failed binding
 * HOW:  Create result message with binding state
 *
 * @param bridge Perception bridge
 * @param binding_id Binding request ID
 * @param result Binding result state
 * @param bound_object_id Resulting object ID (if successful)
 * @param binding_strength Binding strength [0, 1]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_binding_result(
    perception_bio_bridge_t* bridge,
    uint32_t binding_id,
    binding_state_t result,
    uint32_t bound_object_id,
    float binding_strength
);

/**
 * @brief Broadcast multi-modal object percept
 *
 * WHAT: Send unified object percept across modalities
 * WHY:  Provide coherent multi-sensory object representation
 * HOW:  Combine features from multiple modalities
 *
 * @param bridge Perception bridge
 * @param object_id Object ID
 * @param modality_mask Bitmask of contributing modalities
 * @param position Unified position estimate [x, y, z]
 * @param confidence Unified confidence [0, 1]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_multimodal_object(
    perception_bio_bridge_t* bridge,
    uint32_t object_id,
    uint32_t modality_mask,
    const float* position,
    float confidence
);

/* ============================================================================
 * Salience and Attention API
 * ============================================================================ */

/**
 * @brief Broadcast salience map update
 *
 * @param bridge Perception bridge
 * @param modality Source modality
 * @param peak_position Peak salience position [x, y]
 * @param peak_salience Peak salience value
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_salience(
    perception_bio_bridge_t* bridge,
    percept_modality_t modality,
    const float* peak_position,
    float peak_salience
);

/**
 * @brief Broadcast novelty detection
 *
 * @param bridge Perception bridge
 * @param modality Source modality
 * @param novelty_level Novelty level [0, 1]
 * @param position Novelty location [x, y, z]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_novelty(
    perception_bio_bridge_t* bridge,
    percept_modality_t modality,
    float novelty_level,
    const float* position
);

/**
 * @brief Broadcast cross-modal attention shift
 *
 * @param bridge Perception bridge
 * @param source_modality Modality initiating shift
 * @param target_modality Target modality for attention
 * @param position Attention target position [x, y, z]
 * @return 0 on success, error code on failure
 */
int perception_bio_async_broadcast_cross_modal_attention(
    perception_bio_bridge_t* bridge,
    percept_modality_t source_modality,
    percept_modality_t target_modality,
    const float* position
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to perception messages
 *
 * @param bridge Perception bridge
 * @param module_id Subscribing module ID
 * @param modality Modality to subscribe (or PERCEPT_MODALITY_COUNT for all)
 * @param msg_types_low Low 32 bits of message type mask
 * @param msg_types_high High 32 bits of message type mask
 * @return 0 on success, error code on failure
 */
int perception_bio_async_subscribe_module(
    perception_bio_bridge_t* bridge,
    uint32_t module_id,
    percept_modality_t modality,
    uint32_t msg_types_low,
    uint32_t msg_types_high
);

/**
 * @brief Unsubscribe module from perception messages
 *
 * @param bridge Perception bridge
 * @param module_id Module to unsubscribe
 * @param modality Modality to unsubscribe from
 * @return 0 on success, error code on failure
 */
int perception_bio_async_unsubscribe_module(
    perception_bio_bridge_t* bridge,
    uint32_t module_id,
    percept_modality_t modality
);

/**
 * @brief Get subscriber count for message type
 *
 * @param bridge Perception bridge
 * @param msg_type Message type
 * @return Number of subscribers
 */
uint32_t perception_bio_async_get_subscriber_count(
    const perception_bio_bridge_t* bridge,
    percept_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Perception bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int perception_bio_async_get_stats(
    const perception_bio_bridge_t* bridge,
    perception_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Perception bridge
 * @return 0 on success, error code on failure
 */
int perception_bio_async_reset_stats(perception_bio_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return String name of message type
 */
const char* perception_bio_msg_type_name(percept_bio_msg_type_t msg_type);

/**
 * @brief Get modality name
 *
 * @param modality Modality
 * @return String name of modality
 */
const char* perception_modality_name(percept_modality_t modality);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Perception bridge
 */
void perception_bio_async_print_summary(const perception_bio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERCEPTION_BIO_ASYNC_BRIDGE_H */
