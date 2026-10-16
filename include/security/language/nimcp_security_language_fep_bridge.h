/**
 * @file nimcp_security_language_fep_bridge.h
 * @brief Free Energy Principle Bridge for Security Language Processing
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for language security operations (injection detection,
 *       semantic manipulation, context hijacking)
 * WHY:  Malicious language patterns represent high-surprise deviations from
 *       expected linguistic distributions; FEP provides principled detection
 * HOW:  Map language anomalies to free energy, use precision modulation for
 *       sensitivity tuning, employ active inference for protective responses
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * LANGUAGE THREATS AS SURPRISE:
 * -----------------------------
 * In the FEP framework, the brain maintains a generative model of expected language.
 * Malicious language patterns are high-surprise observations that deviate from
 * learned linguistic regularities:
 *
 * - Normal language input = low free energy (expected, predictable patterns)
 * - Prompt injection      = high free energy (unexpected control sequences)
 * - Semantic manipulation = elevated surprise (meaning distortion)
 * - Context hijacking     = extreme surprise (conversation derailment)
 * - Output manipulation   = high free energy (generation corruption)
 *
 * The security system acts as a predictive filter, learning the statistical
 * regularities of legitimate language and flagging high-surprise deviations.
 *
 * FEP INTEGRATION MAPPINGS:
 * -------------------------
 * ```
 * Language Security Metric     -> FEP Metric
 * -------------------------------------------------------------------
 * Injection anomaly score      -> Free energy F
 * Semantic deviation score     -> Prediction error epsilon = o - g(mu)
 * Context hijack severity      -> Surprise -ln p(o)
 * Sanitization/blocking action -> Active inference (action to reduce FE)
 * Detection confidence         -> Precision Pi
 * Threat escalation planning   -> Expected Free Energy (EFE planning)
 * ```
 *
 * PRECISION-WEIGHTED DETECTION:
 * -----------------------------
 * Detection sensitivity is modulated by precision:
 * - High precision = high sensitivity (more detections, potential false positives)
 * - Low precision = low sensitivity (fewer detections, potential false negatives)
 *
 * Precision adapts based on detection feedback:
 * - Confirmed attacks   -> increase precision (heightened alertness)
 * - False positives     -> decrease precision (reduce over-sensitivity)
 * - Normal operations   -> gradual precision decay (relaxation to baseline)
 *
 * ACTIVE INFERENCE FOR LANGUAGE SECURITY:
 * ---------------------------------------
 * The system uses active inference to:
 * 1. PREDICT expected language patterns (generative model)
 * 2. DETECT deviations (prediction errors from input)
 * 3. ACT to minimize free energy (sanitize, block, alert, lockdown)
 * 4. UPDATE generative model from confirmed attacks/normal traffic
 *
 * THREAT CATEGORIES:
 * ==================================================================================
 *
 * PROMPT INJECTION:
 * - Direct injection: Explicit override attempts ("Ignore previous instructions")
 * - Indirect injection: Embedded malicious content in retrieved data
 * - Jailbreak attempts: Persona/roleplay attacks to bypass restrictions
 *
 * SEMANTIC MANIPULATION:
 * - Meaning distortion: Subtle changes to alter interpretation
 * - Sentiment hijacking: Emotional tone manipulation
 * - Topic drift: Gradual steering toward unauthorized topics
 *
 * CONTEXT HIJACKING:
 * - Conversation reset: Attempts to clear/override context
 * - Role confusion: Convincing system to adopt different identity
 * - Memory poisoning: Corrupting cached context information
 *
 * OUTPUT MANIPULATION:
 * - Generation steering: Influencing output content/format
 * - Exfiltration attempts: Encoding sensitive data in responses
 * - Format injection: Injecting structured data in outputs
 *
 * ARCHITECTURE:
 * ==================================================================================
 * ```
 * +============================================================================+
 * |       SECURITY LANGUAGE - FEP BRIDGE (Surprise-Based Threat Detection)     |
 * +============================================================================+
 * |                                                                            |
 * |   +------------------+              +------------------+                   |
 * |   |  FEP System      |------------->|  Security Lang   |                   |
 * |   |                  |              |  Bridge          |                   |
 * |   | * Free Energy    |              |                  |                   |
 * |   | * Surprise       |              | * Injection Det  |                   |
 * |   | * Precision      |              | * Semantic Anal  |                   |
 * |   | * Active Inf     |              | * Context Check  |                   |
 * |   +------------------+              +------------------+                   |
 * |           |                                  |                             |
 * |           v                                  v                             |
 * |   +----------------------------------------------------------------+       |
 * |   |                 BIDIRECTIONAL EFFECTS                          |       |
 * |   |                                                                |       |
 * |   |  FEP -> Security:                                              |       |
 * |   |    * Free energy -> Threat level                               |       |
 * |   |    * Surprise -> Detection threshold                           |       |
 * |   |    * Precision -> Detection sensitivity                        |       |
 * |   |    * EFE -> Protective action selection                        |       |
 * |   |                                                                |       |
 * |   |  Security -> FEP:                                              |       |
 * |   |    * Injection detected -> High-surprise observation           |       |
 * |   |    * Normal input -> Update generative model                   |       |
 * |   |    * False positives -> Reduce precision                       |       |
 * |   |    * Context hijack -> Maximum surprise signal                 |       |
 * |   +----------------------------------------------------------------+       |
 * |                                                                            |
 * +============================================================================+
 * ```
 *
 * REFERENCES:
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 * - Bogacz, R. (2017) "A tutorial on the free-energy framework"
 * - Perez & Ribeiro (2022) "Ignore This Title and HackAPrompt: Prompt Injection"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_LANGUAGE_FEP_BRIDGE_H
#define NIMCP_SECURITY_LANGUAGE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base infrastructure - MUST come first for struct embedding */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security language bridge */
#include "security/language/nimcp_security_language_bridge.h"

/* FEP module */
#include "cognitive/free_energy/nimcp_free_energy.h"

/* Bio-async integration */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

/* Utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Free energy thresholds for language threat levels */
#define SEC_LANG_FEP_NORMAL_FE_THRESHOLD        2.0f   /**< Normal language pattern */
#define SEC_LANG_FEP_SUSPICIOUS_FE_THRESHOLD    5.0f   /**< Suspicious deviation */
#define SEC_LANG_FEP_THREAT_FE_THRESHOLD        10.0f  /**< Active threat detected */
#define SEC_LANG_FEP_CRITICAL_FE_THRESHOLD      20.0f  /**< Critical security event */

/** Precision bounds for detection sensitivity */
#define SEC_LANG_FEP_MIN_PRECISION              0.1f   /**< Minimum sensitivity */
#define SEC_LANG_FEP_MAX_PRECISION              10.0f  /**< Maximum sensitivity */
#define SEC_LANG_FEP_DEFAULT_PRECISION          1.0f   /**< Baseline sensitivity */

/** Surprise thresholds for language anomalies */
#define SEC_LANG_FEP_LOW_SURPRISE               2.0f   /**< Minor deviation */
#define SEC_LANG_FEP_MEDIUM_SURPRISE            5.0f   /**< Moderate deviation */
#define SEC_LANG_FEP_HIGH_SURPRISE              10.0f  /**< Severe deviation */
#define SEC_LANG_FEP_EXTREME_SURPRISE           20.0f  /**< Complete anomaly */

/** Bio-async module identification */
#define BIO_MODULE_SEC_LANG_FEP                 0x4031 /**< Security Language FEP bridge */
#define SEC_LANG_FEP_MODULE_NAME                "security_language_fep_bridge"

/** Default learning rates */
#define SEC_LANG_FEP_DEFAULT_BELIEF_LR          0.05f  /**< Belief update rate */
#define SEC_LANG_FEP_DEFAULT_PRECISION_LR       0.02f  /**< Precision adaptation rate */
#define SEC_LANG_FEP_DEFAULT_THREAT_DECAY       0.95f  /**< Threat level decay rate */

/** Threat category weights for free energy computation */
#define SEC_LANG_FEP_PROMPT_INJECTION_WEIGHT    1.5f   /**< Weight for prompt injection */
#define SEC_LANG_FEP_SEMANTIC_MANIP_WEIGHT      1.2f   /**< Weight for semantic manipulation */
#define SEC_LANG_FEP_CONTEXT_HIJACK_WEIGHT      2.0f   /**< Weight for context hijacking */
#define SEC_LANG_FEP_OUTPUT_MANIP_WEIGHT        1.3f   /**< Weight for output manipulation */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief FEP-based language threat classification
 *
 * WHAT: Threat levels derived from free energy analysis of language patterns
 * WHY:  Categorical threat assessment for response selection
 * HOW:  Map normalized free energy to discrete threat levels
 */
typedef enum {
    SEC_LANG_FEP_THREAT_NONE = 0,      /**< No threat detected (low FE) */
    SEC_LANG_FEP_THREAT_LOW,           /**< Minor anomaly (slightly elevated FE) */
    SEC_LANG_FEP_THREAT_MEDIUM,        /**< Suspicious pattern (moderate FE) */
    SEC_LANG_FEP_THREAT_HIGH,          /**< Active threat (high FE) */
    SEC_LANG_FEP_THREAT_CRITICAL       /**< Critical attack (extreme FE) */
} sec_lang_fep_threat_level_t;

/**
 * @brief Language threat categories
 *
 * WHAT: Classification of language-based security threats
 * WHY:  Different threats require different detection and response strategies
 * HOW:  Each category maps to specific detection patterns and FE weights
 */
typedef enum {
    SEC_LANG_FEP_CATEGORY_NONE = 0,            /**< No threat category */
    SEC_LANG_FEP_CATEGORY_PROMPT_INJECTION,    /**< Prompt injection attempt */
    SEC_LANG_FEP_CATEGORY_SEMANTIC_MANIPULATION,/**< Semantic manipulation */
    SEC_LANG_FEP_CATEGORY_CONTEXT_HIJACKING,   /**< Context hijacking */
    SEC_LANG_FEP_CATEGORY_OUTPUT_MANIPULATION, /**< Output manipulation attempt */
    SEC_LANG_FEP_CATEGORY_JAILBREAK,           /**< Jailbreak/persona attack */
    SEC_LANG_FEP_CATEGORY_DATA_EXFILTRATION,   /**< Data exfiltration attempt */
    SEC_LANG_FEP_CATEGORY_FORMAT_INJECTION,    /**< Format/structure injection */
    SEC_LANG_FEP_CATEGORY_COUNT                /**< Number of categories */
} sec_lang_fep_threat_category_t;

/**
 * @brief Security response actions (active inference)
 *
 * WHAT: Actions to reduce free energy in language security context
 * WHY:  Active inference requires action selection to minimize EFE
 * HOW:  Map threat level to appropriate protective response
 */
typedef enum {
    SEC_LANG_FEP_ACTION_NONE = 0,      /**< No action needed */
    SEC_LANG_FEP_ACTION_LOG,           /**< Log for review */
    SEC_LANG_FEP_ACTION_SANITIZE,      /**< Sanitize input */
    SEC_LANG_FEP_ACTION_THROTTLE,      /**< Rate limit source */
    SEC_LANG_FEP_ACTION_BLOCK,         /**< Block input entirely */
    SEC_LANG_FEP_ACTION_ALERT,         /**< Alert security team */
    SEC_LANG_FEP_ACTION_LOCKDOWN,      /**< Full security lockdown */
    SEC_LANG_FEP_ACTION_COUNT          /**< Number of actions */
} sec_lang_fep_action_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security Language FEP bridge configuration
 *
 * WHAT: Configuration parameters for FEP-language security integration
 * WHY:  Tune sensitivity, learning rates, and response thresholds
 * HOW:  Set thresholds and enable/disable features per threat category
 */
typedef struct {
    /* FEP parameters */
    float injection_fe_threshold;          /**< FE threshold for injection detection */
    float semantic_fe_threshold;           /**< FE threshold for semantic manipulation */
    float context_fe_threshold;            /**< FE threshold for context hijacking */
    float output_fe_threshold;             /**< FE threshold for output manipulation */
    float precision_learning_rate;         /**< Rate for precision adaptation */
    float belief_learning_rate;            /**< Rate for generative model update */

    /* Detection sensitivity */
    float initial_precision;               /**< Starting precision level */
    bool enable_precision_modulation;      /**< Adapt precision from feedback */
    bool enable_fep_scoring;               /**< Use FEP for threat scoring */

    /* Category-specific enables */
    bool enable_prompt_injection_detection;   /**< Detect prompt injection */
    bool enable_semantic_manipulation_detection; /**< Detect semantic manipulation */
    bool enable_context_hijacking_detection;  /**< Detect context hijacking */
    bool enable_output_manipulation_detection;/**< Detect output manipulation */

    /* Active inference settings */
    bool enable_active_inference;          /**< Enable action selection */
    float action_threshold;                /**< EFE threshold for action */
    float threat_decay_rate;               /**< Decay rate for threat levels */

    /* Learning settings */
    bool enable_online_learning;           /**< Update model from detections */
    bool learn_from_false_positives;       /**< Reduce precision on FP */
    bool learn_from_confirmed_attacks;     /**< Increase precision on TP */

    /* Integration settings */
    bool enable_bio_async;                 /**< Enable bio-async messaging */
    bool enable_detailed_logging;          /**< Verbose logging */
} sec_lang_fep_config_t;

/* ============================================================================
 * Effects Structures (Bidirectional Flow)
 * ============================================================================ */

/**
 * @brief FEP to Security Language effects
 *
 * WHAT: Effects flowing from FEP system to language security operations
 * WHY:  FEP metrics modulate security sensitivity and thresholds
 * HOW:  Map free energy, surprise, precision to security parameters
 */
typedef struct {
    /* Threat assessment */
    float free_energy_score;               /**< Current free energy [0-inf) */
    float surprise_score;                  /**< Surprise level [0-inf) */
    float normalized_threat;               /**< Normalized threat [0-1] */
    sec_lang_fep_threat_level_t threat_level; /**< Categorical threat level */

    /* Per-category free energy */
    float prompt_injection_fe;             /**< FE for prompt injection */
    float semantic_manipulation_fe;        /**< FE for semantic manipulation */
    float context_hijacking_fe;            /**< FE for context hijacking */
    float output_manipulation_fe;          /**< FE for output manipulation */

    /* Detection modulation */
    float detection_sensitivity;           /**< Precision-based sensitivity [0-1] */
    float injection_threshold_adj;         /**< Injection threshold adjustment */
    float semantic_threshold_adj;          /**< Semantic threshold adjustment */
    float context_threshold_adj;           /**< Context threshold adjustment */

    /* Recommended actions */
    sec_lang_fep_action_t recommended_action;  /**< Suggested response action */
    float action_urgency;                  /**< How urgent is action [0-1] */
    sec_lang_fep_threat_category_t primary_threat; /**< Primary threat category */

    /* Confidence metrics */
    float detection_confidence;            /**< Confidence in current assessment */
    float model_certainty;                 /**< Certainty of generative model */
} fep_to_sec_lang_effects_t;

/**
 * @brief Security Language to FEP effects
 *
 * WHAT: Effects flowing from language security to FEP system
 * WHY:  Security detections become observations for FEP
 * HOW:  Convert security events to prediction errors and surprise signals
 */
typedef struct {
    /* Detection feedback */
    uint64_t inputs_analyzed;              /**< Total inputs processed */
    uint64_t injections_detected;          /**< Prompt injection attempts found */
    uint64_t semantic_anomalies;           /**< Semantic manipulation attempts */
    uint64_t context_hijacks;              /**< Context hijacking attempts */
    uint64_t output_manipulations;         /**< Output manipulation attempts */

    /* Score aggregates */
    float avg_injection_score;             /**< Average injection score */
    float avg_semantic_score;              /**< Average semantic anomaly */
    float avg_context_score;               /**< Average context deviation */
    float avg_output_score;                /**< Average output anomaly */

    /* False positive tracking */
    uint64_t false_positives;              /**< Known false positives */
    float estimated_precision;             /**< Estimated detection precision */

    /* Pattern statistics */
    float input_complexity_avg;            /**< Average input complexity */
    float semantic_deviation_avg;          /**< Average semantic deviation */
    bool anomaly_trend_rising;             /**< Are anomalies increasing? */

    /* Timing */
    uint64_t last_detection_time_ms;       /**< Timestamp of last detection */
    uint64_t last_attack_time_ms;          /**< Timestamp of last confirmed attack */
} sec_lang_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Per-category threat tracking
 *
 * WHAT: Track threat metrics per category
 * WHY:  Enable fine-grained monitoring and response
 */
typedef struct {
    sec_lang_fep_threat_category_t category; /**< Threat category */
    float current_fe;                      /**< Current free energy for category */
    float peak_fe;                         /**< Peak FE observed */
    uint64_t detection_count;              /**< Total detections */
    uint64_t last_detection_ms;            /**< Last detection timestamp */
    float detection_rate;                  /**< Detections per minute */
} sec_lang_fep_category_state_t;

/**
 * @brief Bridge operational state
 *
 * WHAT: Current state of FEP-language security bridge
 * WHY:  Track active processing and connection status
 * HOW:  Monitor connections, FEP state, and threat tracking
 */
typedef struct {
    bool active;                           /**< Bridge is operational */
    bool fep_connected;                    /**< FEP system connected */
    bool sec_lang_connected;               /**< Security language bridge connected */

    /* Current FEP state */
    float current_free_energy;             /**< Current free energy level */
    float current_surprise;                /**< Current surprise level */
    float current_precision;               /**< Current precision level */

    /* Threat tracking */
    sec_lang_fep_threat_level_t current_threat; /**< Current threat level */
    sec_lang_fep_threat_category_t primary_category; /**< Primary threat category */
    uint64_t threat_start_time_ms;         /**< When current threat began */
    float threat_peak;                     /**< Peak threat level seen */

    /* Per-category state */
    sec_lang_fep_category_state_t category_states[SEC_LANG_FEP_CATEGORY_COUNT];

    /* Update tracking */
    uint64_t last_update_time_ms;          /**< Last update timestamp */
    uint64_t update_count;                 /**< Total updates performed */
} sec_lang_fep_state_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Cumulative statistics for bridge operation
 * WHY:  Monitor effectiveness and tune parameters
 * HOW:  Track processing counts, detection rates, FEP metrics
 */
typedef struct {
    /* Processing counts */
    uint64_t total_updates;                /**< Total update cycles */
    uint64_t fep_computations;             /**< FEP computations performed */
    uint64_t action_selections;            /**< Active inference selections */

    /* Detection statistics */
    uint64_t threats_detected;             /**< Total threats via FEP */
    uint64_t threats_by_level[5];          /**< Threats per level */
    uint64_t threats_by_category[SEC_LANG_FEP_CATEGORY_COUNT]; /**< Per category */
    uint64_t actions_taken[SEC_LANG_FEP_ACTION_COUNT]; /**< Actions by type */

    /* FEP metrics */
    float avg_free_energy;                 /**< Average free energy */
    float avg_surprise;                    /**< Average surprise */
    float avg_precision;                   /**< Average precision */
    float max_free_energy;                 /**< Maximum FE observed */
    float max_surprise;                    /**< Maximum surprise observed */

    /* Learning statistics */
    uint64_t precision_adaptations;        /**< Precision adjustments made */
    uint64_t belief_updates;               /**< Generative model updates */
    uint64_t false_positive_corrections;   /**< FP-based corrections */

    /* Performance */
    float avg_update_time_ms;              /**< Average update time */
    float max_update_time_ms;              /**< Maximum update time */
    uint64_t total_processing_time_ms;     /**< Total processing time */
} sec_lang_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Language FEP Bridge
 *
 * WHAT: FEP integration for language security operations
 * WHY:  Use predictive processing for language threat detection
 * HOW:  Connect FEP system to security language bridge
 *
 * NOTE: bridge_base_t MUST be the first member for proper casting
 */
typedef struct {
    /* Base bridge - MUST be first member */
    bridge_base_t base;

    /* Connected systems */
    fep_system_t* fep_system;              /**< Connected FEP system */
    security_language_bridge_t* sec_lang;  /**< Connected security language bridge */

    /* Configuration */
    sec_lang_fep_config_t config;          /**< Bridge configuration */

    /* Bidirectional effects */
    fep_to_sec_lang_effects_t fep_effects;    /**< FEP->Security effects */
    sec_lang_to_fep_effects_t sec_effects;    /**< Security->FEP effects */

    /* State and statistics */
    sec_lang_fep_state_t state;            /**< Current state */
    sec_lang_fep_stats_t stats;            /**< Cumulative statistics */
} sec_lang_fep_bridge_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for FEP-language security integration
 * WHY:  Easy initialization with balanced sensitivity
 * HOW:  Return pre-configured structure with moderate thresholds
 *
 * @param config Output configuration structure (must not be NULL)
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_default_config(sec_lang_fep_config_t* config);

/**
 * @brief Get current configuration
 *
 * WHAT: Retrieve current bridge configuration
 * WHY:  Allow inspection of active settings
 * HOW:  Copy current config to output
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_get_config(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime tuning of parameters
 * HOW:  Apply new config with validation
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_set_config(
    sec_lang_fep_bridge_t* bridge,
    const sec_lang_fep_config_t* config
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create security language FEP bridge
 *
 * WHAT: Allocate and initialize FEP-language security bridge
 * WHY:  Enable surprise-based threat detection for language processing
 * HOW:  Allocate structure, initialize base, connect systems
 *
 * @param config Configuration (NULL for defaults)
 * @param sec_lang Security language bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
sec_lang_fep_bridge_t* sec_lang_fep_create(
    const sec_lang_fep_config_t* config,
    security_language_bridge_t* sec_lang,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security language FEP bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect bio-async, cleanup base, free memory
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_lang_fep_destroy(sec_lang_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset bridge to initial state
 * WHY:  Clear accumulated state for fresh start
 * HOW:  Zero state, reset precision to default
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_reset(sec_lang_fep_bridge_t* bridge);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on language security
 *
 * WHAT: Calculate FEP-derived security modulation
 * WHY:  Map free energy to threat assessment
 * HOW:  Get FEP state, compute threat level, set sensitivity
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_compute_effects(sec_lang_fep_bridge_t* bridge);

/**
 * @brief Update from injection detection
 *
 * WHAT: Process injection detection as FEP observation
 * WHY:  Feed security events back to FEP system
 * HOW:  Convert detection to prediction error, update beliefs
 *
 * @param bridge Bridge handle
 * @param detection Detection result from security language bridge
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_update_from_detection(
    sec_lang_fep_bridge_t* bridge,
    const security_language_detection_result_t* detection
);

/**
 * @brief Update from semantic analysis
 *
 * WHAT: Process semantic anomaly as FEP observation
 * WHY:  Semantic manipulation informs threat model
 * HOW:  Convert semantic scores to prediction error
 *
 * @param bridge Bridge handle
 * @param semantic_score Semantic anomaly score [0-1]
 * @param context_score Context deviation score [0-1]
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_update_from_semantic(
    sec_lang_fep_bridge_t* bridge,
    float semantic_score,
    float context_score
);

/**
 * @brief Update from output validation
 *
 * WHAT: Process output validation as FEP observation
 * WHY:  Output manipulation attempts inform threat model
 * HOW:  Convert validation result to prediction error
 *
 * @param bridge Bridge handle
 * @param validation Output validation result
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_update_from_output(
    sec_lang_fep_bridge_t* bridge,
    const security_language_output_validation_t* validation
);

/**
 * @brief Full bidirectional update cycle
 *
 * WHAT: Perform complete update in both directions
 * WHY:  Convenience for regular update cycles
 * HOW:  Compute effects, apply modulation, update model
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_update(sec_lang_fep_bridge_t* bridge);

/* ============================================================================
 * Active Inference API
 * ============================================================================ */

/**
 * @brief Select security action via active inference
 *
 * WHAT: Choose action to minimize expected free energy
 * WHY:  Active inference for security response
 * HOW:  Evaluate EFE for each action, select minimum
 *
 * @param bridge Bridge handle
 * @param action Output: selected action
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_select_action(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_action_t* action
);

/**
 * @brief Get expected free energy for action
 *
 * WHAT: Compute EFE for specific action
 * WHY:  Evaluate action before selection
 * HOW:  Project future states, compute expected surprise
 *
 * @param bridge Bridge handle
 * @param action Action to evaluate
 * @param efe Output: expected free energy
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_get_action_efe(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_action_t action,
    float* efe
);

/**
 * @brief Apply recommended action
 *
 * WHAT: Execute the recommended security action
 * WHY:  Complete active inference loop
 * HOW:  Apply action to security language bridge
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_apply_action(sec_lang_fep_bridge_t* bridge);

/* ============================================================================
 * Precision Modulation API
 * ============================================================================ */

/**
 * @brief Apply precision modulation
 *
 * WHAT: Adjust detection sensitivity via precision
 * WHY:  Adapt to current threat environment
 * HOW:  Scale thresholds based on precision level
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_apply_precision_modulation(sec_lang_fep_bridge_t* bridge);

/**
 * @brief Report false positive
 *
 * WHAT: Report detection as false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Decrease precision proportionally
 *
 * @param bridge Bridge handle
 * @param category Category of false positive (or NONE for general)
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_report_false_positive(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category
);

/**
 * @brief Report confirmed attack
 *
 * WHAT: Report detection as confirmed true positive
 * WHY:  Increase precision for heightened alertness
 * HOW:  Increase precision, update generative model
 *
 * @param bridge Bridge handle
 * @param category Category of confirmed attack
 * @param severity Attack severity [0-1]
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_report_confirmed_attack(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category,
    float severity
);

/**
 * @brief Set precision level directly
 *
 * WHAT: Override precision to specific value
 * WHY:  Manual sensitivity tuning
 * HOW:  Clamp to valid range and apply
 *
 * @param bridge Bridge handle
 * @param precision New precision value [MIN, MAX]
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_set_precision(
    sec_lang_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Get current precision
 *
 * @param bridge Bridge handle
 * @return Current precision or -1.0 on error
 */
float sec_lang_fep_get_precision(const sec_lang_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP to security effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_get_fep_effects(
    const sec_lang_fep_bridge_t* bridge,
    fep_to_sec_lang_effects_t* effects
);

/**
 * @brief Get security to FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_get_sec_effects(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_to_fep_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_get_state(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_get_stats(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_stats_t* stats
);

/**
 * @brief Get current threat level
 *
 * @param bridge Bridge handle
 * @return Current threat level or SEC_LANG_FEP_THREAT_NONE on error
 */
sec_lang_fep_threat_level_t sec_lang_fep_get_threat_level(
    const sec_lang_fep_bridge_t* bridge
);

/**
 * @brief Get primary threat category
 *
 * @param bridge Bridge handle
 * @return Primary threat category or SEC_LANG_FEP_CATEGORY_NONE on error
 */
sec_lang_fep_threat_category_t sec_lang_fep_get_primary_category(
    const sec_lang_fep_bridge_t* bridge
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0 on error
 */
float sec_lang_fep_get_free_energy(const sec_lang_fep_bridge_t* bridge);

/**
 * @brief Get current surprise
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1.0 on error
 */
float sec_lang_fep_get_surprise(const sec_lang_fep_bridge_t* bridge);

/**
 * @brief Get free energy for specific category
 *
 * @param bridge Bridge handle
 * @param category Threat category
 * @return Category-specific free energy or -1.0 on error
 */
float sec_lang_fep_get_category_fe(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_connect_bio_async(sec_lang_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_disconnect_bio_async(sec_lang_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool sec_lang_fep_is_bio_async_connected(const sec_lang_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages from bio-async inbox
 * WHY:  Handle async security notifications
 * HOW:  Use bio_router_process_inbox to handle pending messages
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, -1 on error
 */
int sec_lang_fep_process_messages(sec_lang_fep_bridge_t* bridge);

/**
 * @brief Broadcast threat alert via bio-async
 *
 * WHAT: Send threat notification to other modules
 * WHY:  Coordinate security response across system
 * HOW:  Create and send bio-async message
 *
 * @param bridge Bridge handle
 * @param threat_level Threat level to broadcast
 * @param category Threat category
 * @return 0 on success, -1 on error
 */
int sec_lang_fep_broadcast_threat(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_level_t threat_level,
    sec_lang_fep_threat_category_t category
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Print bridge summary
 *
 * WHAT: Output human-readable bridge summary
 * WHY:  Debugging and monitoring
 * HOW:  Format state and stats to logging
 *
 * @param bridge Bridge handle
 */
void sec_lang_fep_print_summary(const sec_lang_fep_bridge_t* bridge);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear cumulative statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero out stats structure
 *
 * @param bridge Bridge handle
 */
void sec_lang_fep_reset_stats(sec_lang_fep_bridge_t* bridge);

/**
 * @brief Get threat level name
 *
 * @param level Threat level
 * @return Human-readable name string
 */
const char* sec_lang_fep_threat_level_name(sec_lang_fep_threat_level_t level);

/**
 * @brief Get threat category name
 *
 * @param category Threat category
 * @return Human-readable name string
 */
const char* sec_lang_fep_category_name(sec_lang_fep_threat_category_t category);

/**
 * @brief Get action name
 *
 * @param action Action type
 * @return Human-readable name string
 */
const char* sec_lang_fep_action_name(sec_lang_fep_action_t action);

/**
 * @brief Convert FEP threat level to security language severity
 *
 * @param level FEP threat level
 * @return Equivalent security language threat severity
 */
security_language_threat_severity_t sec_lang_fep_to_severity(
    sec_lang_fep_threat_level_t level
);

/**
 * @brief Convert security language injection type to FEP category
 *
 * @param injection_type Security language injection type
 * @return Equivalent FEP threat category
 */
sec_lang_fep_threat_category_t sec_lang_fep_from_injection_type(
    security_language_injection_type_t injection_type
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_LANGUAGE_FEP_BRIDGE_H */
