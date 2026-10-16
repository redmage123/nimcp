/**
 * @file nimcp_security_collective_fep_bridge.h
 * @brief Free Energy Principle bridge for Security-Collective Cognition Integration
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for collective cognition security
 * WHY:  Consensus violations and Sybil attacks represent high-surprise events
 *       in the predictive processing framework - system expects legitimate
 *       collective behavior, deviations generate free energy to be minimized
 * HOW:  Map Byzantine detection, consensus deviation, and swarm manipulation
 *       to free energy metrics; use active inference for security responses
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * COLLECTIVE IMMUNE RESPONSE AS FEP:
 * -----------------------------------
 * - Social insect colonies detect invaders via chemical "surprise" signals
 * - Bacterial quorum sensing expects predictable population patterns
 * - Neural synchronization has predictable phase relationships
 * - Deviation from expected collective behavior = high free energy
 *
 * FEP INTEGRATION FOR COLLECTIVE SECURITY:
 * ----------------------------------------
 * ```
 * Collective Observation (o) → Behavior Extraction
 *         ↓
 * Expected Swarm Pattern μ (generative model of legitimate behavior)
 *         ↓
 * Prediction Error: ε = o - g(μ)
 *   - Byzantine behavior deviation
 *   - Consensus manipulation signals
 *   - Sybil attack indicators
 *         ↓
 * Free Energy F = Complexity + Inaccuracy
 *         ↓
 * Surprise = -ln p(o) ≤ F
 *         ↓
 * Security Score = F / F_threshold
 * ```
 *
 * THREAT MAPPING TO FEP:
 * ----------------------
 * - Low FE (<2.0) → Normal collective behavior
 * - Medium FE (2-5) → Suspicious patterns (monitor)
 * - High FE (5-10) → Byzantine/Sybil indicators (alert)
 * - Very high FE (>10) → Active attack (quarantine)
 *
 * PRECISION MODULATION:
 * ---------------------
 * - High precision = Sensitive detection (may increase false positives)
 * - Low precision = Tolerant detection (may miss attacks)
 * - Adaptive precision based on threat landscape
 *
 * ACTIVE INFERENCE RESPONSES:
 * ---------------------------
 * - Quarantine Byzantine agents (minimize expected FE)
 * - Adjust consensus thresholds (update generative model)
 * - Strengthen identity verification (reduce Sybil surprise)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║        SECURITY-COLLECTIVE COGNITION FEP BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌──────────────────┐         ┌──────────────────┐                       ║
 * ║   │  FEP System      │────────▶│  Security        │                       ║
 * ║   │                  │         │  Collective      │                       ║
 * ║   │ • Free Energy    │         │  Bridge          │                       ║
 * ║   │ • Surprise       │         │                  │                       ║
 * ║   │ • Precision      │         │ • Byzantine Det  │                       ║
 * ║   │ • Active Inf     │         │ • Consensus Ver  │                       ║
 * ║   └──────────────────┘         │ • Sybil Detect   │                       ║
 * ║           ↓                    └──────────────────┘                       ║
 * ║   ┌──────────────────────────────────────────────────────────────┐       ║
 * ║   │              BIDIRECTIONAL EFFECTS                           │       ║
 * ║   │                                                              │       ║
 * ║   │  FEP → Security:                                             │       ║
 * ║   │    • Free energy → Threat severity                           │       ║
 * ║   │    • Surprise → Byzantine probability                        │       ║
 * ║   │    • Precision → Detection sensitivity                       │       ║
 * ║   │    • Active inference → Quarantine decisions                 │       ║
 * ║   │                                                              │       ║
 * ║   │  Security → FEP:                                             │       ║
 * ║   │    • Consensus deviation → High-surprise observation         │       ║
 * ║   │    • Sybil detection → Update identity priors                │       ║
 * ║   │    • Trust scores → Modulate agent precision                 │       ║
 * ║   │    • False positives → Reduce detection precision            │       ║
 * ║   └──────────────────────────────────────────────────────────────┘       ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_COLLECTIVE_FEP_BRIDGE_H
#define NIMCP_SECURITY_COLLECTIVE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/collective/nimcp_security_collective_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module identifier for this bridge */
#define BIO_MODULE_SECURITY_COLLECTIVE_FEP  0x0640

/** Free energy thresholds for collective security */
#define COLLECTIVE_FEP_NORMAL_THRESHOLD       2.0f   /**< Normal collective behavior */
#define COLLECTIVE_FEP_SUSPICIOUS_THRESHOLD   5.0f   /**< Suspicious patterns */
#define COLLECTIVE_FEP_BYZANTINE_THRESHOLD    10.0f  /**< Byzantine indicators */
#define COLLECTIVE_FEP_ATTACK_THRESHOLD       20.0f  /**< Active attack */

/** Precision bounds for detection sensitivity */
#define COLLECTIVE_FEP_MIN_PRECISION          0.1f   /**< Minimum detection precision */
#define COLLECTIVE_FEP_MAX_PRECISION          10.0f  /**< Maximum detection precision */
#define COLLECTIVE_FEP_DEFAULT_PRECISION      1.0f   /**< Default precision level */

/** Consensus deviation scaling */
#define COLLECTIVE_FEP_CONSENSUS_WEIGHT       2.0f   /**< Weight for consensus deviation */
#define COLLECTIVE_FEP_BYZANTINE_WEIGHT       3.0f   /**< Weight for Byzantine behavior */
#define COLLECTIVE_FEP_SYBIL_WEIGHT           4.0f   /**< Weight for Sybil indicators */

/** Active inference thresholds */
#define COLLECTIVE_FEP_QUARANTINE_THRESHOLD   0.75f  /**< FE ratio triggering quarantine */
#define COLLECTIVE_FEP_RESTORE_THRESHOLD      0.25f  /**< FE ratio for restoration */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief FEP-based threat classification
 */
typedef enum {
    COLLECTIVE_FEP_THREAT_NONE = 0,      /**< No threat (low FE) */
    COLLECTIVE_FEP_THREAT_MONITOR,       /**< Requires monitoring (medium FE) */
    COLLECTIVE_FEP_THREAT_BYZANTINE,     /**< Byzantine behavior (high FE) */
    COLLECTIVE_FEP_THREAT_SYBIL,         /**< Sybil attack (very high FE) */
    COLLECTIVE_FEP_THREAT_CRITICAL       /**< Critical attack (extreme FE) */
} collective_fep_threat_t;

/**
 * @brief Active inference response type
 */
typedef enum {
    COLLECTIVE_FEP_RESPONSE_NONE = 0,    /**< No action needed */
    COLLECTIVE_FEP_RESPONSE_OBSERVE,     /**< Increase observation precision */
    COLLECTIVE_FEP_RESPONSE_WARN,        /**< Issue warning to collective */
    COLLECTIVE_FEP_RESPONSE_ISOLATE,     /**< Isolate suspicious agent */
    COLLECTIVE_FEP_RESPONSE_QUARANTINE,  /**< Full quarantine */
    COLLECTIVE_FEP_RESPONSE_RESTORE      /**< Restore from quarantine */
} collective_fep_response_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief FEP bridge configuration for collective security
 */
typedef struct {
    /* FEP parameters */
    float byzantine_fe_threshold;        /**< FE threshold for Byzantine detection */
    float sybil_fe_threshold;            /**< FE threshold for Sybil detection */
    float consensus_fe_threshold;        /**< FE threshold for consensus violation */
    float surprise_threshold;            /**< General surprise threshold */

    /* Detection parameters */
    bool use_fep_scoring;                /**< Use FEP for threat scoring */
    bool enable_precision_modulation;    /**< Adapt precision dynamically */
    float consensus_deviation_weight;    /**< Weight for consensus deviation in FE */
    float byzantine_behavior_weight;     /**< Weight for Byzantine behavior in FE */
    float sybil_indicator_weight;        /**< Weight for Sybil indicators in FE */

    /* Active inference */
    bool enable_active_inference;        /**< Enable active inference responses */
    float quarantine_threshold;          /**< FE ratio for quarantine decision */
    float restore_threshold;             /**< FE ratio for restoration */
    float inference_learning_rate;       /**< Learning rate for inference */

    /* Precision learning */
    float precision_learning_rate;       /**< Rate for precision adaptation */
    bool learn_from_false_positives;     /**< Reduce precision on FP feedback */
    bool learn_from_true_positives;      /**< Increase precision on TP feedback */

    /* Sensitivity factors */
    float fep_sensitivity;               /**< FEP effect scaling [0.5-2.0] */
    float security_sensitivity;          /**< Security effect scaling [0.5-2.0] */
} security_collective_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on collective security (FEP → Security)
 *
 * WHAT: Effects computed from FEP state that influence security decisions
 * WHY:  Free energy quantifies how "surprising" collective behavior is
 * HOW:  Map FE components to security metrics
 */
typedef struct {
    /* Free energy derived metrics */
    float total_free_energy;             /**< Current total FE */
    float consensus_surprise;            /**< Surprise from consensus deviation */
    float byzantine_surprise;            /**< Surprise from Byzantine behavior */
    float sybil_surprise;                /**< Surprise from Sybil indicators */

    /* Derived security scores */
    float threat_severity;               /**< Normalized threat severity [0-1] */
    float byzantine_probability;         /**< Probability of Byzantine attack */
    float sybil_probability;             /**< Probability of Sybil attack */
    float consensus_confidence;          /**< Confidence in consensus validity */

    /* Precision effects */
    float detection_sensitivity;         /**< Precision-derived sensitivity */
    float current_precision;             /**< Current precision level */

    /* Threat classification */
    collective_fep_threat_t threat_level;        /**< Classified threat level */
    collective_fep_response_t recommended_action; /**< Active inference recommendation */
} fep_to_security_effects_t;

/**
 * @brief Security effects on FEP (Security → FEP)
 *
 * WHAT: Security observations that update FEP generative model
 * WHY:  Detected threats are high-surprise observations
 * HOW:  Feed security metrics back to update beliefs
 */
typedef struct {
    /* Detection events */
    uint64_t byzantine_detections;       /**< Total Byzantine detections */
    uint64_t sybil_detections;           /**< Total Sybil detections */
    uint64_t consensus_violations;       /**< Total consensus violations */
    uint64_t false_positives;            /**< Known false positives */

    /* Trust-based precision modulation */
    float avg_agent_trust;               /**< Average agent trust score */
    float trust_variance;                /**< Variance in trust scores */
    uint32_t untrusted_agent_count;      /**< Count of untrusted agents */

    /* Consensus metrics */
    float avg_consensus_deviation;       /**< Average consensus deviation */
    float max_consensus_deviation;       /**< Maximum deviation seen */
    float consensus_health;              /**< Overall consensus health [0-1] */

    /* Quarantine state */
    uint32_t quarantined_agents;         /**< Currently quarantined agents */
    uint32_t restored_agents;            /**< Agents restored from quarantine */
} security_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current FEP bridge state
 */
typedef struct {
    bool active;                         /**< Whether bridge is active */
    uint64_t update_count;               /**< Number of updates */
    uint64_t inference_count;            /**< Active inference actions */

    /* Running FEP state */
    float current_free_energy;           /**< Current total FE */
    float avg_free_energy;               /**< Running average FE */
    float max_free_energy;               /**< Maximum FE observed */
    float current_surprise;              /**< Current surprise level */
    float avg_surprise;                  /**< Running average surprise */

    /* Precision state */
    float consensus_precision;           /**< Precision for consensus monitoring */
    float byzantine_precision;           /**< Precision for Byzantine detection */
    float sybil_precision;               /**< Precision for Sybil detection */

    /* Active inference state */
    collective_fep_response_t last_response;  /**< Last active inference response */
    uint64_t last_quarantine_time;       /**< Timestamp of last quarantine */
    uint64_t last_restore_time;          /**< Timestamp of last restoration */
} security_collective_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Update statistics */
    uint64_t total_updates;              /**< Total bridge updates */
    uint64_t fep_based_decisions;        /**< Decisions guided by FEP */

    /* Detection statistics */
    uint64_t threats_detected;           /**< Total threats detected */
    uint64_t byzantine_detections;       /**< Byzantine detections */
    uint64_t sybil_detections;           /**< Sybil detections */
    uint64_t consensus_violations;       /**< Consensus violations */
    uint64_t false_positives;            /**< Known false positives */

    /* Active inference statistics */
    uint64_t quarantine_actions;         /**< Quarantine actions taken */
    uint64_t restore_actions;            /**< Restoration actions taken */
    uint64_t warning_actions;            /**< Warning actions taken */
    uint64_t precision_adaptations;      /**< Precision adaptations */

    /* Free energy statistics */
    float avg_free_energy;               /**< Average free energy */
    float max_free_energy;               /**< Maximum free energy */
    float avg_surprise;                  /**< Average surprise */
    float current_precision;             /**< Current overall precision */

    /* Performance */
    float avg_update_time_ns;            /**< Average update time */
    float avg_inference_time_ns;         /**< Average inference time */
} security_collective_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Collective FEP Bridge
 *
 * WHAT: Bridges security-collective integration with FEP system
 * WHY:  Enables predictive processing for collective threat detection
 * HOW:  Maps security observations to free energy, uses active inference
 */
typedef struct security_collective_fep_bridge {
    bridge_base_t base;                  /**< MUST be first: base bridge */

    /* Configuration */
    security_collective_fep_config_t config;

    /* Connected systems */
    security_collective_bridge_t* security_bridge; /**< Security-collective bridge */
    fep_system_t* fep_system;            /**< FEP system */

    /* Bidirectional effects */
    fep_to_security_effects_t fep_effects;      /**< FEP → Security effects */
    security_to_fep_effects_t security_effects; /**< Security → FEP effects */

    /* State */
    security_collective_fep_state_t state;

    /* Statistics */
    security_collective_fep_stats_t stats;
} security_collective_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provides sensible defaults for collective security FEP integration
 * WHY:  Simplifies initialization with biologically-plausible values
 * HOW:  Sets thresholds based on expected collective behavior variance
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int security_collective_fep_default_config(security_collective_fep_config_t* config);

/**
 * @brief Create security-collective FEP bridge
 *
 * WHAT: Initialize FEP integration for collective security
 * WHY:  Enable surprise-based threat detection in collective systems
 * HOW:  Connect FEP system to security bridge, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @param security_bridge Security-collective bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
security_collective_fep_bridge_t* security_collective_fep_create(
    const security_collective_fep_config_t* config,
    security_collective_bridge_t* security_bridge,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security-collective FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks and dangling connections
 * HOW:  Disconnect bio-async, cleanup base, free bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_collective_fep_destroy(security_collective_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Reset state and statistics while preserving connections
 * WHY:  Allow bridge reuse without reconnection overhead
 * HOW:  Zero state/stats, reset precision to defaults
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_collective_fep_reset(security_collective_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration structure
 * @return 0 on success, error code on failure
 */
int security_collective_fep_get_config(
    const security_collective_fep_bridge_t* bridge,
    security_collective_fep_config_t* config
);

/**
 * @brief Set bridge configuration
 *
 * WHAT: Update bridge configuration at runtime
 * WHY:  Allow dynamic tuning of detection parameters
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, error code on failure
 */
int security_collective_fep_set_config(
    security_collective_fep_bridge_t* bridge,
    const security_collective_fep_config_t* config
);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on security (main update)
 *
 * WHAT: Calculate free energy from current collective state
 * WHY:  Core FEP computation - quantify surprise from collective behavior
 * HOW:  Process security observations through generative model
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_collective_fep_compute_effects(security_collective_fep_bridge_t* bridge);

/**
 * @brief Update FEP from security detection
 *
 * WHAT: Feed security detection result to FEP system
 * WHY:  Detections are high-surprise observations that update beliefs
 * HOW:  Convert detection to observation, process through FEP
 *
 * @param bridge Bridge handle
 * @param detection_type Type of detection (byzantine/sybil/consensus)
 * @param agent_id Affected agent ID (0 for collective)
 * @param severity Detection severity [0-1]
 * @return 0 on success, error code on failure
 */
int security_collective_fep_update_from_detection(
    security_collective_fep_bridge_t* bridge,
    collective_fep_threat_t detection_type,
    uint32_t agent_id,
    float severity
);

/**
 * @brief Apply active inference for security response
 *
 * WHAT: Use active inference to determine security action
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate response policies, select action that minimizes EFE
 *
 * @param bridge Bridge handle
 * @param response Output recommended response
 * @return 0 on success, error code on failure
 */
int security_collective_fep_active_inference(
    security_collective_fep_bridge_t* bridge,
    collective_fep_response_t* response
);

/**
 * @brief Apply precision modulation
 *
 * WHAT: Adapt detection precision based on threat landscape
 * WHY:  Precision is attention - focus on likely threats
 * HOW:  Update precision based on detection history and false positive rate
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_collective_fep_modulate_precision(
    security_collective_fep_bridge_t* bridge
);

/* ============================================================================
 * Detection Feedback API
 * ============================================================================ */

/**
 * @brief Report consensus deviation to FEP
 *
 * WHAT: Feed consensus deviation metric to FEP
 * WHY:  Deviation from expected consensus is surprise
 * HOW:  Map deviation to prediction error, update beliefs
 *
 * @param bridge Bridge handle
 * @param deviation Consensus deviation score [0-1]
 * @param participant_count Number of consensus participants
 * @return 0 on success, error code on failure
 */
int security_collective_fep_report_consensus(
    security_collective_fep_bridge_t* bridge,
    float deviation,
    uint32_t participant_count
);

/**
 * @brief Report Byzantine behavior detection
 *
 * WHAT: Feed Byzantine detection to FEP
 * WHY:  Byzantine agents generate high-surprise observations
 * HOW:  Map detection to prediction error with Byzantine weight
 *
 * @param bridge Bridge handle
 * @param agent_id Agent exhibiting Byzantine behavior
 * @param confidence Detection confidence [0-1]
 * @return 0 on success, error code on failure
 */
int security_collective_fep_report_byzantine(
    security_collective_fep_bridge_t* bridge,
    uint32_t agent_id,
    float confidence
);

/**
 * @brief Report Sybil attack indicators
 *
 * WHAT: Feed Sybil indicators to FEP
 * WHY:  Fake identities violate expected identity distribution
 * HOW:  Map Sybil probability to prediction error with high weight
 *
 * @param bridge Bridge handle
 * @param suspected_count Number of suspected Sybil agents
 * @param probability Sybil attack probability [0-1]
 * @return 0 on success, error code on failure
 */
int security_collective_fep_report_sybil(
    security_collective_fep_bridge_t* bridge,
    uint32_t suspected_count,
    float probability
);

/**
 * @brief Report false positive for precision learning
 *
 * WHAT: Indicate that a detection was a false positive
 * WHY:  Reduces precision to prevent similar false alarms
 * HOW:  Lower precision for the detection type that triggered FP
 *
 * @param bridge Bridge handle
 * @param detection_type Type of false positive detection
 * @return 0 on success, error code on failure
 */
int security_collective_fep_report_false_positive(
    security_collective_fep_bridge_t* bridge,
    collective_fep_threat_t detection_type
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_collective_fep_get_fep_effects(
    const security_collective_fep_bridge_t* bridge,
    fep_to_security_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_collective_fep_get_security_effects(
    const security_collective_fep_bridge_t* bridge,
    security_to_fep_effects_t* effects
);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int security_collective_fep_get_state(
    const security_collective_fep_bridge_t* bridge,
    security_collective_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int security_collective_fep_get_stats(
    const security_collective_fep_bridge_t* bridge,
    security_collective_fep_stats_t* stats
);

/**
 * @brief Get current free energy level
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0f on error
 */
float security_collective_fep_get_free_energy(
    const security_collective_fep_bridge_t* bridge
);

/**
 * @brief Get current threat level
 *
 * @param bridge Bridge handle
 * @return Current threat classification
 */
collective_fep_threat_t security_collective_fep_get_threat_level(
    const security_collective_fep_bridge_t* bridge
);

/* ============================================================================
 * Debug/Diagnostic API
 * ============================================================================ */

/**
 * @brief Print bridge summary to stdout
 *
 * WHAT: Display human-readable bridge state summary
 * WHY:  Debugging and monitoring
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void security_collective_fep_print_summary(
    const security_collective_fep_bridge_t* bridge
);

/**
 * @brief Reset statistics only (preserve state)
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_collective_fep_reset_stats(
    security_collective_fep_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module threat notifications
 * HOW:  Register module, setup message inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_collective_fep_connect_bio_async(
    security_collective_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_collective_fep_disconnect_bio_async(
    security_collective_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected to bio-async router
 */
bool security_collective_fep_is_bio_async_connected(
    const security_collective_fep_bridge_t* bridge
);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages from bio-async inbox
 * WHY:  Handle threat notifications from other modules
 * HOW:  Dequeue and process each message
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, -1 on error
 */
int security_collective_fep_process_messages(
    security_collective_fep_bridge_t* bridge
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert threat level to string
 *
 * @param threat Threat level
 * @return Human-readable string
 */
const char* collective_fep_threat_to_string(collective_fep_threat_t threat);

/**
 * @brief Convert response type to string
 *
 * @param response Response type
 * @return Human-readable string
 */
const char* collective_fep_response_to_string(collective_fep_response_t response);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_COLLECTIVE_FEP_BRIDGE_H */
