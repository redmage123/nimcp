/**
 * @file nimcp_security_memory_fep_bridge.h
 * @brief Free Energy Principle bridge for Security Memory Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for memory system security across all memory types
 * WHY:  Memory anomalies are high-surprise events in the FEP framework
 * HOW:  Map memory security metrics to free energy, use prediction errors for detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MEMORY SECURITY AS SURPRISE DETECTION:
 * - Intact memory = low free energy (expected access patterns)
 * - Memory corruption = high free energy (unexpected data changes)
 * - False memory injection = prediction error (unexpected content)
 * - Memory replay attacks = high surprise (violates temporal model)
 * - Retrieval manipulation = model violation (unexpected retrieval patterns)
 *
 * FEP INTEGRATION:
 * ```
 * Memory Access Observation (o) -> Security Assessment
 *         |
 * Expected Memory Behavior mu (learned generative model)
 *         |
 * Prediction Error: epsilon = o - g(mu)
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Surprise = -ln p(o) <= F
 *         |
 * Security Threat Level = F / F_threshold
 * ```
 *
 * SECURITY-FEP MAPPING:
 * - Memory integrity score -> Free energy (inverted: low integrity = high FE)
 * - Access pattern anomaly -> Prediction error
 * - Content tampering -> Surprise level
 * - Protective response -> Active inference action
 *
 * ATTACK DETECTION MAPPING:
 * - Memory Corruption: High FE from integrity deviation
 * - False Memory Injection: High surprise from unexpected content
 * - Replay Attacks: Temporal prediction errors
 * - Retrieval Manipulation: Access pattern anomalies
 *
 * THREAT LEVEL MAPPING:
 * - Low FE (<2.0) -> Normal memory operations
 * - Medium FE (2-5) -> Suspicious activity (monitor closely)
 * - High FE (5-10) -> Memory attack detected
 * - Very high FE (>10) -> Critical memory tampering
 *
 * ARCHITECTURE:
 * ```
 * +============================================================================+
 * |         SECURITY MEMORY - FEP BRIDGE (Multi-Memory Attack Detection)      |
 * +============================================================================+
 * |                                                                            |
 * |   +------------------+         +----------------------+                    |
 * |   |  FEP System      |-------->|  Security Memory     |                    |
 * |   |                  |         |  Bridge              |                    |
 * |   | * Free Energy    |         |                      |                    |
 * |   | * Surprise       |         | * Working Memory     |                    |
 * |   | * Precision      |         | * Episodic Memory    |                    |
 * |   |                  |         | * Semantic Memory    |                    |
 * |   |                  |         | * Procedural Memory  |                    |
 * |   +------------------+         +----------------------+                    |
 * |           |                              |                                 |
 * |   +--------------------------------------------------------------+         |
 * |   |              BIDIRECTIONAL EFFECTS                           |         |
 * |   |                                                              |         |
 * |   |  FEP -> Security:                                            |         |
 * |   |    * Free energy -> Threat level                             |         |
 * |   |    * Surprise -> Anomaly detection sensitivity               |         |
 * |   |    * Precision -> Detection confidence                       |         |
 * |   |                                                              |         |
 * |   |  Security -> FEP:                                            |         |
 * |   |    * Detected attacks -> High-surprise observations          |         |
 * |   |    * Normal operations -> Update generative model            |         |
 * |   |    * False positives -> Reduce precision                     |         |
 * |   +--------------------------------------------------------------+         |
 * |                                                                            |
 * +============================================================================+
 * ```
 *
 * ACTIVE INFERENCE FOR PROTECTIVE RESPONSES:
 * - High free energy triggers protective measures
 * - Precision modulation adjusts detection sensitivity
 * - Belief updates adapt to evolving attack patterns
 * - Active inference selects optimal countermeasures
 *
 * MULTI-MEMORY SYSTEM COVERAGE:
 * - Working Memory: Short-term data integrity
 * - Episodic Memory: Autobiographical event protection
 * - Semantic Memory: Fact/knowledge tampering detection
 * - Procedural Memory: Skill/habit manipulation detection
 *
 * @see nimcp_security_memory_bridge.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_MEMORY_FEP_BRIDGE_H
#define NIMCP_SECURITY_MEMORY_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/memory/nimcp_security_memory_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Free energy thresholds for memory security threat levels */
#define SEC_MEM_FEP_NORMAL_THRESHOLD        2.0f   /**< Below = normal operations */
#define SEC_MEM_FEP_SUSPICIOUS_THRESHOLD    5.0f   /**< Above = suspicious activity */
#define SEC_MEM_FEP_ATTACK_THRESHOLD        10.0f  /**< Above = attack detected */
#define SEC_MEM_FEP_CRITICAL_THRESHOLD      20.0f  /**< Above = critical threat */

/** Precision bounds for detection sensitivity */
#define SEC_MEM_FEP_MIN_PRECISION           0.1f   /**< Minimum precision */
#define SEC_MEM_FEP_MAX_PRECISION           10.0f  /**< Maximum precision */
#define SEC_MEM_FEP_DEFAULT_PRECISION       1.0f   /**< Default precision */

/** Surprise thresholds for anomaly detection */
#define SEC_MEM_FEP_SURPRISE_NORMAL         2.0f   /**< Normal surprise level */
#define SEC_MEM_FEP_SURPRISE_ANOMALY        8.0f   /**< Anomaly threshold */

/** Prediction error thresholds */
#define SEC_MEM_FEP_PE_TOLERANCE            0.2f   /**< Normal prediction error */
#define SEC_MEM_FEP_PE_ATTACK               0.7f   /**< Attack-level prediction error */

/** Bio-async module ID for security-memory FEP bridge */
#define BIO_MODULE_SECURITY_MEMORY_FEP      0x0E11

/** Attack type detection weights */
#define SEC_MEM_FEP_CORRUPTION_WEIGHT       0.35f  /**< Weight for corruption detection */
#define SEC_MEM_FEP_INJECTION_WEIGHT        0.30f  /**< Weight for false memory injection */
#define SEC_MEM_FEP_REPLAY_WEIGHT           0.20f  /**< Weight for replay attack detection */
#define SEC_MEM_FEP_RETRIEVAL_WEIGHT        0.15f  /**< Weight for retrieval manipulation */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Memory security threat levels based on FEP metrics
 *
 * WHAT: Threat classification from free energy analysis
 * WHY:  Enable graded response to memory security events
 */
typedef enum {
    SEC_MEM_FEP_THREAT_NONE = 0,          /**< No threat detected */
    SEC_MEM_FEP_THREAT_SUSPICIOUS,        /**< Suspicious pattern */
    SEC_MEM_FEP_THREAT_MODERATE,          /**< Moderate threat */
    SEC_MEM_FEP_THREAT_HIGH,              /**< High threat level */
    SEC_MEM_FEP_THREAT_CRITICAL           /**< Critical - immediate response */
} sec_mem_fep_threat_level_t;

/**
 * @brief Memory attack types detected via FEP
 *
 * WHAT: Types of memory attacks detectable through FEP analysis
 * WHY:  Different attacks produce different FEP signatures
 */
typedef enum {
    SEC_MEM_FEP_ATTACK_NONE = 0,          /**< No attack detected */
    SEC_MEM_FEP_ATTACK_CORRUPTION,        /**< Memory corruption detected */
    SEC_MEM_FEP_ATTACK_FALSE_INJECTION,   /**< False memory injection */
    SEC_MEM_FEP_ATTACK_REPLAY,            /**< Memory replay attack */
    SEC_MEM_FEP_ATTACK_RETRIEVAL_MANIP,   /**< Retrieval manipulation */
    SEC_MEM_FEP_ATTACK_MULTIPLE           /**< Multiple attack types */
} sec_mem_fep_attack_type_t;

/**
 * @brief Active inference response types
 *
 * WHAT: Types of protective responses via active inference
 * WHY:  Different threats require different countermeasures
 */
typedef enum {
    SEC_MEM_FEP_RESPONSE_NONE = 0,        /**< No response needed */
    SEC_MEM_FEP_RESPONSE_MONITOR,         /**< Increase monitoring */
    SEC_MEM_FEP_RESPONSE_VERIFY,          /**< Verify memory integrity */
    SEC_MEM_FEP_RESPONSE_QUARANTINE,      /**< Quarantine affected memory */
    SEC_MEM_FEP_RESPONSE_PROTECT,         /**< Activate protection mode */
    SEC_MEM_FEP_RESPONSE_ISOLATE,         /**< Isolate memory region */
    SEC_MEM_FEP_RESPONSE_RESTORE,         /**< Restore from secure state */
    SEC_MEM_FEP_RESPONSE_LOCKDOWN         /**< Full memory lockdown */
} sec_mem_fep_response_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Security Memory FEP bridge configuration
 *
 * WHAT: Configuration for FEP-security memory integration
 * WHY:  Control detection sensitivity and response parameters
 * HOW:  Adjustable thresholds, learning rates, and feature flags
 */
typedef struct {
    /* FEP parameters */
    float free_energy_threshold;          /**< FE threshold for threat detection */
    float surprise_threshold;             /**< Surprise threshold for anomaly */
    float precision_learning_rate;        /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_detection;               /**< Use FEP for threat detection */
    bool enable_precision_modulation;     /**< Adapt precision dynamically */
    float normal_fe_threshold;            /**< FE threshold for normal ops */
    float critical_fe_threshold;          /**< FE threshold for critical */

    /* Attack-specific detection */
    bool detect_corruption;               /**< Enable corruption detection */
    bool detect_false_injection;          /**< Enable false memory detection */
    bool detect_replay_attacks;           /**< Enable replay attack detection */
    bool detect_retrieval_manipulation;   /**< Enable retrieval manipulation detection */

    /* Memory integrity mapping */
    float integrity_to_fe_scale;          /**< Scale factor: integrity -> FE */
    float access_pattern_pe_weight;       /**< Weight for access pattern errors */
    float content_surprise_weight;        /**< Weight for content surprises */
    float temporal_pe_weight;             /**< Weight for temporal prediction errors */

    /* Per-memory system weights */
    float working_memory_weight;          /**< Detection weight for working memory */
    float episodic_memory_weight;         /**< Detection weight for episodic memory */
    float semantic_memory_weight;         /**< Detection weight for semantic memory */
    float procedural_memory_weight;       /**< Detection weight for procedural memory */

    /* Active inference settings */
    bool enable_active_inference;         /**< Enable protective responses */
    float response_threshold;             /**< FE threshold for response */
    float action_temperature;             /**< Softmax temperature for actions */

    /* Learning */
    bool enable_online_learning;          /**< Update FEP from detections */
    float learning_rate;                  /**< Belief update rate */
    bool learn_from_false_positives;      /**< Reduce precision on FP */

    /* Bio-async integration */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} sec_mem_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on security memory (FEP -> Security)
 *
 * WHAT: How FEP analysis affects security operations
 * WHY:  Free energy provides threat detection signal
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                    /**< Current free energy */
    float surprise_level;                 /**< Current surprise */
    float prediction_error;               /**< Prediction error magnitude */

    /* Threat assessment */
    sec_mem_fep_threat_level_t threat_level;  /**< Derived threat level */
    float threat_confidence;              /**< Threat detection confidence [0-1] */
    sec_mem_fep_attack_type_t attack_type;    /**< Detected attack type */

    /* Per-attack type confidence */
    float corruption_confidence;          /**< Confidence in corruption detection */
    float injection_confidence;           /**< Confidence in injection detection */
    float replay_confidence;              /**< Confidence in replay detection */
    float retrieval_confidence;           /**< Confidence in retrieval manipulation */

    /* Detection sensitivity */
    float detection_sensitivity;          /**< Precision-based sensitivity */
    float integrity_estimate;             /**< FEP-derived integrity estimate */

    /* Per-memory system metrics */
    float working_mem_fe;                 /**< Working memory free energy */
    float episodic_mem_fe;                /**< Episodic memory free energy */
    float semantic_mem_fe;                /**< Semantic memory free energy */
    float procedural_mem_fe;              /**< Procedural memory free energy */

    /* Recommended response */
    sec_mem_fep_response_t recommended_response;  /**< Recommended action */
    float response_urgency;               /**< Response urgency [0-1] */
} fep_to_sec_mem_effects_t;

/**
 * @brief Security Memory effects on FEP (Security -> FEP)
 *
 * WHAT: How security detections affect FEP beliefs
 * WHY:  Security events update the generative model
 */
typedef struct {
    /* Detection statistics */
    uint64_t attacks_detected;            /**< Total attacks detected */
    uint64_t normal_operations;           /**< Normal operations counted */
    uint64_t false_positives;             /**< Known false positives */

    /* Per-attack type counts */
    uint64_t corruption_attacks;          /**< Corruption attacks detected */
    uint64_t injection_attacks;           /**< False memory injections detected */
    uint64_t replay_attacks;              /**< Replay attacks detected */
    uint64_t retrieval_attacks;           /**< Retrieval manipulations detected */

    /* Memory health metrics */
    float avg_integrity_score;            /**< Average memory integrity */
    float access_pattern_health;          /**< Access pattern health [0-1] */
    float content_validity;               /**< Content validation score [0-1] */
    float temporal_consistency;           /**< Temporal consistency score [0-1] */

    /* Per-memory system health */
    float working_mem_health;             /**< Working memory health [0-1] */
    float episodic_mem_health;            /**< Episodic memory health [0-1] */
    float semantic_mem_health;            /**< Semantic memory health [0-1] */
    float procedural_mem_health;          /**< Procedural memory health [0-1] */

    /* Current threat status */
    float current_threat_level;           /**< Current security threat [0-1] */
    bool under_attack;                    /**< Active attack in progress */
    uint64_t last_attack_time;            /**< Timestamp of last attack */
} sec_mem_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief FEP bridge state
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track real-time status for monitoring and debugging
 */
typedef struct {
    bool active;                          /**< Whether bridge is active */
    uint64_t update_count;                /**< Number of updates */
    uint64_t detection_count;             /**< Detections processed */
    uint64_t last_update_time;            /**< Last update timestamp */

    float current_precision;              /**< Current precision level */
    float avg_surprise;                   /**< Running average surprise */
    float avg_prediction_error;           /**< Running average PE */

    sec_mem_fep_threat_level_t last_threat;   /**< Last detected threat */
    sec_mem_fep_attack_type_t last_attack;    /**< Last attack type */
    uint64_t last_threat_time;            /**< Timestamp of last threat */
} sec_mem_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    /* Update statistics */
    uint64_t total_updates;               /**< Total updates performed */
    uint64_t fep_detections;              /**< FEP-based detections */
    uint64_t threats_detected;            /**< Threats found */
    uint64_t protective_responses;        /**< Protective actions taken */
    uint64_t precision_adaptations;       /**< Precision updates */

    /* Per-attack type detection counts */
    uint64_t corruption_detections;       /**< Corruption attacks detected */
    uint64_t injection_detections;        /**< Injection attacks detected */
    uint64_t replay_detections;           /**< Replay attacks detected */
    uint64_t retrieval_detections;        /**< Retrieval manipulations detected */

    /* FEP metric averages */
    float avg_free_energy;                /**< Average free energy */
    float avg_surprise;                   /**< Average surprise */
    float avg_prediction_error;           /**< Average prediction error */
    float current_precision;              /**< Current precision */

    /* Maximum values observed */
    float max_free_energy;                /**< Maximum FE observed */
    float max_surprise;                   /**< Maximum surprise observed */

    /* Detection performance */
    uint64_t false_positive_count;        /**< False positives */
    uint64_t true_positive_count;         /**< True positives */
    float detection_accuracy;             /**< Detection accuracy rate */

    /* Per-memory system statistics */
    uint64_t working_mem_detections;      /**< Working memory detections */
    uint64_t episodic_mem_detections;     /**< Episodic memory detections */
    uint64_t semantic_mem_detections;     /**< Semantic memory detections */
    uint64_t procedural_mem_detections;   /**< Procedural memory detections */
} sec_mem_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Memory FEP Bridge
 *
 * WHAT: Main bridge structure connecting security memory to FEP
 * WHY:  Centralized integration of memory security with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                   /**< MUST be first: base bridge infrastructure */

    sec_mem_fep_config_t config;          /**< Configuration */

    /* System connections */
    security_mem_bridge_t* security_mem;  /**< Connected security memory bridge */
    fep_system_t* fep_system;             /**< Connected FEP system */

    /* Bidirectional effects */
    fep_to_sec_mem_effects_t fep_effects;     /**< FEP -> Security effects */
    sec_mem_to_fep_effects_t security_effects; /**< Security -> FEP effects */

    /* State and statistics */
    sec_mem_fep_state_t state;            /**< Current state */
    sec_mem_fep_stats_t stats;            /**< Statistics */
} sec_mem_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for security-FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 * HOW:  Return balanced defaults for detection and learning
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_default_config(sec_mem_fep_config_t* config);

/**
 * @brief Create security memory FEP bridge
 *
 * WHAT: Initialize FEP integration for memory security
 * WHY:  Enable surprise-based threat detection across all memory types
 * HOW:  Connect FEP system to security memory, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param security_mem Security memory bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
sec_mem_fep_bridge_t* sec_mem_fep_create(
    const sec_mem_fep_config_t* config,
    security_mem_bridge_t* security_mem,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security memory FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_mem_fep_destroy(sec_mem_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state while preserving connections
 * WHY:  Fresh start without reconnection
 * HOW:  Reset effects, state, and statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_reset(sec_mem_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_get_config(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime tuning of parameters
 * HOW:  Copy new config, validate bounds
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_set_config(
    sec_mem_fep_bridge_t* bridge,
    const sec_mem_fep_config_t* config
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on security
 *
 * WHAT: Compute FEP-derived security metrics
 * WHY:  Use free energy for threat detection
 * HOW:  Process current FEP state, compute effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_compute_effects(sec_mem_fep_bridge_t* bridge);

/**
 * @brief Update from security detection
 *
 * WHAT: Feed security detection back to FEP
 * WHY:  Update generative model from detections
 * HOW:  Convert detection to FEP observation, update beliefs
 *
 * @param bridge Bridge handle
 * @param attack_detected Whether attack was detected
 * @param attack_type Type of attack detected
 * @param integrity_score Current memory integrity score [0-1]
 * @param memory_type Memory system type affected
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_update_from_detection(
    sec_mem_fep_bridge_t* bridge,
    bool attack_detected,
    sec_mem_fep_attack_type_t attack_type,
    float integrity_score,
    security_mem_system_type_t memory_type
);

/**
 * @brief Update full bridge state
 *
 * WHAT: Main update loop for bridge synchronization
 * WHY:  Maintain bidirectional synchronization
 * HOW:  Compute effects in both directions, update state
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_update(sec_mem_fep_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * Attack Detection API
 * ============================================================================ */

/**
 * @brief Detect memory corruption attack
 *
 * WHAT: Analyze memory state for corruption using FEP
 * WHY:  Corruption produces high free energy from integrity deviation
 * HOW:  Compare current state to expected, compute prediction error
 *
 * @param bridge Bridge handle
 * @param integrity_score Memory integrity score [0-1]
 * @param memory_type Memory system type
 * @param threat_level_out Output threat level
 * @param confidence_out Output detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_detect_corruption(
    sec_mem_fep_bridge_t* bridge,
    float integrity_score,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
);

/**
 * @brief Detect false memory injection attack
 *
 * WHAT: Analyze for false memory injection using FEP
 * WHY:  Injected memories produce high surprise (unexpected content)
 * HOW:  Check content consistency against generative model
 *
 * @param bridge Bridge handle
 * @param content_score Content validation score [0-1]
 * @param source_validity Source memory validity score [0-1]
 * @param memory_type Memory system type
 * @param threat_level_out Output threat level
 * @param confidence_out Output detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_detect_false_injection(
    sec_mem_fep_bridge_t* bridge,
    float content_score,
    float source_validity,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
);

/**
 * @brief Detect memory replay attack
 *
 * WHAT: Analyze for replay attacks using FEP
 * WHY:  Replay attacks produce temporal prediction errors
 * HOW:  Check temporal consistency, detect duplicate patterns
 *
 * @param bridge Bridge handle
 * @param temporal_score Temporal consistency score [0-1]
 * @param replay_detected Whether replay pattern was detected
 * @param memory_type Memory system type
 * @param threat_level_out Output threat level
 * @param confidence_out Output detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_detect_replay_attack(
    sec_mem_fep_bridge_t* bridge,
    float temporal_score,
    bool replay_detected,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
);

/**
 * @brief Detect retrieval manipulation attack
 *
 * WHAT: Analyze for retrieval manipulation using FEP
 * WHY:  Manipulated retrieval produces access pattern anomalies
 * HOW:  Check retrieval patterns against expected model
 *
 * @param bridge Bridge handle
 * @param retrieval_score Retrieval accuracy score [0-1]
 * @param access_pattern_score Access pattern normality score [0-1]
 * @param memory_type Memory system type
 * @param threat_level_out Output threat level
 * @param confidence_out Output detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_detect_retrieval_manipulation(
    sec_mem_fep_bridge_t* bridge,
    float retrieval_score,
    float access_pattern_score,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
);

/**
 * @brief Comprehensive threat detection
 *
 * WHAT: Analyze memory state for all attack types using FEP
 * WHY:  Combined analysis for complete threat assessment
 * HOW:  Run all detectors, compute combined free energy
 *
 * @param bridge Bridge handle
 * @param integrity_score Memory integrity score [0-1]
 * @param content_score Content validation score [0-1]
 * @param temporal_score Temporal consistency score [0-1]
 * @param retrieval_score Retrieval accuracy score [0-1]
 * @param memory_type Memory system type
 * @param threat_level_out Output threat level
 * @param attack_type_out Output detected attack type
 * @param confidence_out Output detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_detect_threat(
    sec_mem_fep_bridge_t* bridge,
    float integrity_score,
    float content_score,
    float temporal_score,
    float retrieval_score,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    sec_mem_fep_attack_type_t* attack_type_out,
    float* confidence_out
);

/* ============================================================================
 * Response API
 * ============================================================================ */

/**
 * @brief Get recommended protective response
 *
 * WHAT: Determine appropriate response via active inference
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate response policies, select optimal action
 *
 * @param bridge Bridge handle
 * @param response_out Output recommended response
 * @param urgency_out Output urgency level [0-1]
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_get_response(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_response_t* response_out,
    float* urgency_out
);

/**
 * @brief Get response for specific attack type
 *
 * WHAT: Get tailored response for specific attack
 * WHY:  Different attacks need different countermeasures
 * HOW:  Select response based on attack type and severity
 *
 * @param bridge Bridge handle
 * @param attack_type Attack type to respond to
 * @param severity Attack severity [0-1]
 * @param response_out Output recommended response
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_get_attack_response(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_attack_type_t attack_type,
    float severity,
    sec_mem_fep_response_t* response_out
);

/**
 * @brief Report false positive detection
 *
 * WHAT: Update FEP on known false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Lower precision for observation type
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_report_false_positive(sec_mem_fep_bridge_t* bridge);

/**
 * @brief Report true positive detection
 *
 * WHAT: Update FEP on confirmed attack
 * WHY:  Increase precision for confirmed detections
 * HOW:  Boost precision, update model
 *
 * @param bridge Bridge handle
 * @param attack_type Confirmed attack type
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_report_true_positive(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_attack_type_t attack_type
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_get_fep_effects(
    const sec_mem_fep_bridge_t* bridge,
    fep_to_sec_mem_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_get_security_effects(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_to_fep_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_get_state(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_get_stats(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_stats_t* stats
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0f on error
 */
float sec_mem_fep_get_free_energy(const sec_mem_fep_bridge_t* bridge);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1.0f on error
 */
float sec_mem_fep_get_surprise(const sec_mem_fep_bridge_t* bridge);

/**
 * @brief Get current threat level
 *
 * @param bridge Bridge handle
 * @return Current threat level
 */
sec_mem_fep_threat_level_t sec_mem_fep_get_threat_level(
    const sec_mem_fep_bridge_t* bridge
);

/**
 * @brief Get current attack type
 *
 * @param bridge Bridge handle
 * @return Current detected attack type
 */
sec_mem_fep_attack_type_t sec_mem_fep_get_attack_type(
    const sec_mem_fep_bridge_t* bridge
);

/**
 * @brief Get free energy for specific memory type
 *
 * @param bridge Bridge handle
 * @param memory_type Memory system type
 * @return Free energy for memory type or -1.0f on error
 */
float sec_mem_fep_get_memory_type_fe(
    const sec_mem_fep_bridge_t* bridge,
    security_mem_system_type_t memory_type
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
int sec_mem_fep_connect_bio_async(sec_mem_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_disconnect_bio_async(sec_mem_fep_bridge_t* bridge);

/**
 * @brief Process bio-async messages
 *
 * WHAT: Process incoming bio-async messages
 * WHY:  Handle security notifications from other modules
 * HOW:  Use bio_router_process_inbox() for message processing
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, -1 on error
 */
int sec_mem_fep_process_messages(sec_mem_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool sec_mem_fep_is_bio_async_connected(const sec_mem_fep_bridge_t* bridge);

/**
 * @brief Send security alert via bio-async
 *
 * WHAT: Broadcast security alert to other modules
 * WHY:  Notify system of detected threats
 * HOW:  Send message through bio-async router
 *
 * @param bridge Bridge handle
 * @param threat_level Threat level to broadcast
 * @param attack_type Attack type detected
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_send_alert(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_threat_level_t threat_level,
    sec_mem_fep_attack_type_t attack_type
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get threat level name
 *
 * @param level Threat level
 * @return Human-readable name
 */
const char* sec_mem_fep_threat_level_name(sec_mem_fep_threat_level_t level);

/**
 * @brief Get attack type name
 *
 * @param attack Attack type
 * @return Human-readable name
 */
const char* sec_mem_fep_attack_type_name(sec_mem_fep_attack_type_t attack);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* sec_mem_fep_response_name(sec_mem_fep_response_t response);

/**
 * @brief Print bridge summary
 *
 * WHAT: Print current bridge state (debug)
 * WHY:  Facilitate debugging and monitoring
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void sec_mem_fep_print_summary(const sec_mem_fep_bridge_t* bridge);

/**
 * @brief Print statistics
 *
 * @param stats Statistics to print
 */
void sec_mem_fep_print_stats(const sec_mem_fep_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_mem_fep_reset_stats(sec_mem_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_MEMORY_FEP_BRIDGE_H */
