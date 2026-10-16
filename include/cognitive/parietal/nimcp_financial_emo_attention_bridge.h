/**
 * @file nimcp_financial_emo_attention_bridge.h
 * @brief Financial Emotion-Attention Bridge - Emotion-driven attention modulation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modulating attention width and stimulus boosting based on
 *       emotional states. Part of Phase 5 Attention Systems integration.
 *
 * WHY:  Emotions profoundly affect attention allocation in trading:
 *       - Joy broadens attention (see opportunities)
 *       - Fear narrows attention (tunnel vision on threats)
 *       - Greed intensifies focus on profit-related stimuli
 *       - Panic causes dangerous tunnel vision (miss important signals)
 *       This bridge enables:
 *       - Dynamic attention width modulation based on emotional state
 *       - Emotion-congruent stimulus boosting for relevant signals
 *       - Detection of dangerous tunnel vision states
 *       - Integration with salience and decision systems
 *
 * HOW:  Emotional state vector is processed to compute:
 *       - Attention width: broad (joy) to narrow (fear/panic)
 *       - Stimulus boosts: emotion-congruent signals receive priority
 *       - Tunnel vision detection: extreme narrowing triggers alerts
 *       Based on Broaden-and-Build theory and Attentional Narrowing research.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                Financial Emotion-Attention Bridge                         |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |     Emotional State       |       |    Attention Parameters   |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Joy, Fear, Anger          |       | Attention Width [0-1]     |        |
 * |  | Surprise, Sadness         |       | Stimulus Boosts           |        |
 * |  | Greed, Panic              |       | Tunnel Vision Flag        |        |
 * |  +------------+--------------+       +-------------+-------------+        |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +----------------------------------------------------------+            |
 * |  |           Attention Modulation Engine                     |            |
 * |  |  emotion -> width_calc -> boost_calc -> tunnel_detect     |            |
 * |  +----------------------------------------------------------+            |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +---------------------------+       +---------------------------+        |
 * |  |   Broadened Attention     |       |   Tunnel Vision Alert     |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Joy: See opportunities    |       | Panic: Dangerous narrowing|        |
 * |  | Curious: Explore signals  |       | Fear: Miss critical info  |        |
 * |  +---------------------------+       +---------------------------+        |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_emotion_bridge.h
 * @see nimcp_financial_salience_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_EMO_ATTENTION_BRIDGE_H
#define NIMCP_FINANCIAL_EMO_ATTENTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_EMO_ATTENTION_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC      0x4645414D  /* 'FEAM' */

/** Bio-async module ID for financial emotion-attention bridge */
#define BIO_MODULE_FINANCIAL_EMO_ATTENTION        0x03A0

/** Maximum number of stimuli for boosting */
#define FIN_EMO_ATTN_MAX_STIMULI                  256

/** Default attention width (neutral state) */
#define FIN_EMO_ATTN_DEFAULT_WIDTH                0.5f

/** Tunnel vision threshold - below this triggers alert */
#define FIN_EMO_ATTN_TUNNEL_VISION_THRESHOLD      0.2f

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_EMO_ATTN_ERROR_BASE                   33400
#define FIN_EMO_ATTN_ERR_OK                       0
#define FIN_EMO_ATTN_ERR_NULL                     (FIN_EMO_ATTN_ERROR_BASE + 1)
#define FIN_EMO_ATTN_ERR_INVALID_PARAM            (FIN_EMO_ATTN_ERROR_BASE + 2)
#define FIN_EMO_ATTN_ERR_NO_MEMORY                (FIN_EMO_ATTN_ERROR_BASE + 3)
#define FIN_EMO_ATTN_ERR_STATE                    (FIN_EMO_ATTN_ERROR_BASE + 4)
#define FIN_EMO_ATTN_ERR_IMMUNE                   (FIN_EMO_ATTN_ERROR_BASE + 5)
#define FIN_EMO_ATTN_ERR_BBB                      (FIN_EMO_ATTN_ERROR_BASE + 6)
#define FIN_EMO_ATTN_ERR_VALIDATION               (FIN_EMO_ATTN_ERROR_BASE + 7)
#define FIN_EMO_ATTN_ERR_CAPACITY                 (FIN_EMO_ATTN_ERROR_BASE + 8)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Stimulus types for emotion-congruent boosting
 */
typedef enum {
    FIN_STIMULUS_OPPORTUNITY = 0,    /**< Profit opportunity signal */
    FIN_STIMULUS_THREAT,             /**< Risk/loss threat signal */
    FIN_STIMULUS_NOVELTY,            /**< Novel/unexpected signal */
    FIN_STIMULUS_ROUTINE,            /**< Expected/routine signal */
    FIN_STIMULUS_SOCIAL,             /**< Social/crowd behavior signal */
    FIN_STIMULUS_CONFIRMATION,       /**< Confirms existing belief */
    FIN_STIMULUS_CONTRADICTION,      /**< Contradicts existing belief */
    FIN_STIMULUS_NEUTRAL,            /**< Neutral/informational signal */
    FIN_STIMULUS_COUNT
} fin_stimulus_type_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_EMO_ATTN_STATE_UNINITIALIZED = 0,
    FIN_EMO_ATTN_STATE_INITIALIZED,
    FIN_EMO_ATTN_STATE_ACTIVE,
    FIN_EMO_ATTN_STATE_DEGRADED,
    FIN_EMO_ATTN_STATE_ERROR
} fin_emo_attention_bridge_state_t;

/**
 * @brief Tunnel vision severity levels
 */
typedef enum {
    FIN_TUNNEL_VISION_NONE = 0,      /**< Normal attention width */
    FIN_TUNNEL_VISION_MILD,          /**< Slightly narrowed */
    FIN_TUNNEL_VISION_MODERATE,      /**< Significantly narrowed */
    FIN_TUNNEL_VISION_SEVERE,        /**< Dangerous narrowing - alert! */
    FIN_TUNNEL_VISION_CRITICAL       /**< Critical - immediate intervention needed */
} fin_tunnel_vision_severity_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Emotional input for attention modulation
 *
 * All values are in the range [0.0, 1.0] representing intensity.
 */
typedef struct {
    float joy;          /**< Happiness, elation - broadens attention */
    float fear;         /**< Anxiety, terror - narrows attention */
    float anger;        /**< Frustration, rage - narrows on target */
    float surprise;     /**< Astonishment - resets attention */
    float sadness;      /**< Disappointment - reduces attention capacity */
    float greed;        /**< Desire for profit - focuses on opportunities */
    float panic;        /**< Extreme fear - dangerous tunnel vision */
} fin_emotion_input_t;

/**
 * @brief Attention state modulated by emotion
 *
 * Contains current attention parameters derived from emotional state.
 */
typedef struct {
    float attention_width;              /**< Broad (1.0/joy) vs narrow (0.0/fear) */
    float* stimulus_boosts;             /**< Emotion-congruent boosting factors */
    uint32_t num_stimuli;               /**< Number of stimuli being tracked */
    bool tunnel_vision;                 /**< Extreme fear narrowing detected */
} fin_emo_attention_state_t;

/**
 * @brief Tunnel vision detection result
 */
typedef struct {
    bool detected;                      /**< Tunnel vision detected */
    fin_tunnel_vision_severity_t severity;  /**< Severity level */
    float attention_width;              /**< Current attention width */
    float contributing_fear;            /**< Fear contribution */
    float contributing_panic;           /**< Panic contribution */
    char description[256];              /**< Human-readable description */
    bool intervention_recommended;      /**< Should intervene */
} fin_tunnel_vision_result_t;

/**
 * @brief Stimulus with attention boost applied
 */
typedef struct {
    fin_stimulus_type_t type;           /**< Stimulus type */
    float base_salience;                /**< Original salience score */
    float boost_factor;                 /**< Emotion-driven boost factor */
    float boosted_salience;             /**< Final boosted salience */
    char label[64];                     /**< Optional label for stimulus */
} fin_boosted_stimulus_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t modulations;               /**< Total attention modulations */
    uint64_t tunnel_vision_detections;  /**< Tunnel vision detection calls */
    uint64_t immune_checks;             /**< Immune system checks */
    uint64_t bbb_validations;           /**< BBB validations performed */
    uint64_t kg_messages_sent;          /**< KG messages published */
    uint64_t health_heartbeats;         /**< Health heartbeats sent */
} fin_emo_attention_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Attention width modulation parameters
 */
typedef struct {
    float joy_broadening_factor;        /**< How much joy broadens attention [0,1] */
    float fear_narrowing_factor;        /**< How much fear narrows attention [0,1] */
    float anger_narrowing_factor;       /**< How much anger narrows attention [0,1] */
    float panic_narrowing_factor;       /**< How much panic narrows attention [0,1] */
    float sadness_reduction_factor;     /**< How much sadness reduces capacity [0,1] */
} fin_width_modulation_params_t;

/**
 * @brief Stimulus boost parameters
 */
typedef struct {
    float greed_opportunity_boost;      /**< Boost for opportunities when greedy */
    float fear_threat_boost;            /**< Boost for threats when fearful */
    float anger_contradiction_boost;    /**< Boost for contradictions when angry */
    float joy_confirmation_boost;       /**< Boost for confirmations when joyful */
} fin_stimulus_boost_params_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Width modulation */
    fin_width_modulation_params_t width_params;

    /* Stimulus boosting */
    fin_stimulus_boost_params_t boost_params;

    /* Tunnel vision detection */
    float tunnel_vision_threshold;      /**< Width below which tunnel vision detected */
    float critical_threshold;           /**< Width for critical intervention */

    /* Baseline */
    float baseline_width;               /**< Default attention width */
    float width_decay_rate;             /**< Rate of return to baseline */

    /* Integration settings */
    bool enable_immune_integration;     /**< Enable immune system */
    bool enable_bbb_validation;         /**< Enable BBB validation */
    bool enable_kg_messaging;           /**< Enable KG messaging */
    bool enable_health_monitoring;      /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;               /**< Verbose debug output */
} fin_emo_attention_config_t;

/* ============================================================================
 * Forward Declarations for Security Subsystems
 * ============================================================================ */

#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

#ifndef ETHICS_ENGINE_T_DEFINED
#define ETHICS_ENGINE_T_DEFINED
typedef struct ethics_engine_struct* ethics_engine_t;
#endif

#ifndef BRAIN_CYCLE_COORDINATOR_T_DEFINED
#define BRAIN_CYCLE_COORDINATOR_T_DEFINED
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;
#endif

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial emotion-attention bridge handle
 */
typedef struct financial_emo_attention_bridge financial_emo_attention_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_emo_attention_bridge_default_config(fin_emo_attention_config_t* config);

/**
 * @brief Create financial emotion-attention bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_emo_attention_bridge_t* financial_emo_attention_bridge_create(
    const fin_emo_attention_config_t* config
);

/**
 * @brief Destroy financial emotion-attention bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_emo_attention_bridge_destroy(financial_emo_attention_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_reset(financial_emo_attention_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_emo_attention_bridge_set_immune(financial_emo_attention_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_emo_attention_bridge_set_bbb(financial_emo_attention_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_emo_attention_bridge_set_health_agent(financial_emo_attention_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_emo_attention_bridge_set_kg_wiring(financial_emo_attention_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_emo_attention_bridge_set_logger(financial_emo_attention_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_emo_attention_bridge_set_security(financial_emo_attention_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_emo_attention_bridge_set_ethics(financial_emo_attention_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_emo_attention_bridge_set_lgss(financial_emo_attention_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_emo_attention_bridge_set_coordinator(financial_emo_attention_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_emo_attention_bridge_set_bio_router(financial_emo_attention_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Core Attention Modulation API
 * ============================================================================ */

/**
 * @brief Modulate attention based on emotional state
 *
 * Computes attention width and stimulus boost factors based on
 * current emotional state. Updates internal attention state.
 *
 * @param bridge Bridge handle
 * @param emotion Current emotional state
 * @param state Output attention state (optional, can be NULL)
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_modulate(
    financial_emo_attention_bridge_t* bridge,
    const fin_emotion_input_t* emotion,
    fin_emo_attention_state_t* state
);

/**
 * @brief Detect dangerous tunnel vision
 *
 * Analyzes current attention state and emotional inputs to detect
 * dangerous attentional narrowing that could lead to missed signals.
 *
 * @param bridge Bridge handle
 * @param result Output tunnel vision detection result
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_detect_tunnel_vision(
    financial_emo_attention_bridge_t* bridge,
    fin_tunnel_vision_result_t* result
);

/**
 * @brief Apply emotion-congruent boost to stimuli
 *
 * Takes an array of stimuli and applies emotion-driven boosting
 * based on stimulus type and current emotional state.
 *
 * @param bridge Bridge handle
 * @param stimuli Input stimuli array
 * @param count Number of stimuli
 * @param output Output array for boosted stimuli
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_boost_stimuli(
    financial_emo_attention_bridge_t* bridge,
    const fin_boosted_stimulus_t* stimuli,
    size_t count,
    fin_boosted_stimulus_t* output
);

/**
 * @brief Get current attention state
 *
 * @param bridge Bridge handle
 * @param state Output attention state
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_get_state(
    const financial_emo_attention_bridge_t* bridge,
    fin_emo_attention_state_t* state
);

/**
 * @brief Get current attention width
 *
 * @param bridge Bridge handle
 * @return Current attention width [0,1], or -1 on error
 */
float financial_emo_attention_bridge_get_width(
    const financial_emo_attention_bridge_t* bridge
);

/**
 * @brief Decay attention width toward baseline
 *
 * Call periodically to gradually return attention width to baseline
 * as emotional intensity fades.
 *
 * @param bridge Bridge handle
 * @param elapsed_ms Milliseconds since last decay call
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_decay(
    financial_emo_attention_bridge_t* bridge,
    uint64_t elapsed_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
fin_emo_attention_bridge_state_t financial_emo_attention_bridge_get_bridge_state(
    const financial_emo_attention_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_get_stats(
    const financial_emo_attention_bridge_t* bridge,
    fin_emo_attention_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_emo_attention_bridge_reset_stats(financial_emo_attention_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_emo_attention_bridge_get_last_error(void);

/* ============================================================================
 * Health Integration
 * ============================================================================ */

/**
 * @brief Send heartbeat
 *
 * @param bridge Bridge handle
 * @param operation Current operation
 * @param progress Progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_heartbeat(
    financial_emo_attention_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get stimulus type name
 *
 * @param stimulus_type Stimulus type
 * @return String name (static)
 */
const char* fin_emo_attn_stimulus_name(fin_stimulus_type_t stimulus_type);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_emo_attn_state_name(fin_emo_attention_bridge_state_t state);

/**
 * @brief Get tunnel vision severity name
 *
 * @param severity Severity level
 * @return String name (static)
 */
const char* fin_emo_attn_tunnel_vision_name(fin_tunnel_vision_severity_t severity);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_emo_attention_bridge_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_training_begin(financial_emo_attention_bridge_t* bridge);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_training_end(financial_emo_attention_bridge_t* bridge);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_emo_attention_bridge_training_step(financial_emo_attention_bridge_t* bridge, float progress);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_EMO_ATTENTION_BRIDGE_H */
