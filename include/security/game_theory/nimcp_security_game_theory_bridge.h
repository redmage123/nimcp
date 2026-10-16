/**
 * @file nimcp_security_game_theory_bridge.h
 * @brief Security-Game Theory Integration Bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: Bidirectional integration between security subsystem and game theory
 * WHY:  Game-theoretic reasoning is vulnerable to adversarial exploitation,
 *       payoff manipulation, coalition attacks, and equilibrium tampering.
 *       Security provides validation and monitoring; game theory provides
 *       strategic reasoning for threat response.
 * HOW:  Security validates payoff matrices, monitors coalitions, verifies
 *       mechanism design, detects manipulation; game theory provides strategic
 *       analysis of security threat models.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE SYSTEM AND GAME-THEORETIC DEFENSE:
 * ------------------------------------------
 * - T-cells and B-cells form coalitions against pathogens
 * - Immune memory as strategic adaptation to repeated games
 * - Autoimmune disorders as coalition formation gone wrong
 * - Security = immune checkpoint preventing coalition attacks
 *
 * SECURITY -> GAME THEORY PATHWAYS:
 * ----------------------------------
 * 1. Payoff Validation:
 *    - Validate payoff matrix integrity (no tampering)
 *    - Detect NaN/Inf values that could crash equilibrium solvers
 *    - Verify payoff bounds to prevent overflow attacks
 *
 * 2. Coalition Monitoring:
 *    - Monitor coalition formation for malicious patterns
 *    - Detect Sybil attacks in coalition games
 *    - Prevent collusion that undermines mechanism design
 *
 * 3. Mechanism Integrity:
 *    - Verify mechanism design rules are enforced
 *    - Prevent manipulation of allocation/payment rules
 *    - Ensure incentive compatibility is maintained
 *
 * 4. Nash Equilibrium Verification:
 *    - Verify computed equilibria are valid
 *    - Detect adversarially crafted "equilibria"
 *    - Prevent strategy manipulation
 *
 * GAME THEORY -> SECURITY PATHWAYS:
 * ----------------------------------
 * 1. Strategic Threat Analysis:
 *    - Model attacker/defender as game players
 *    - Compute optimal defense strategies
 *    - Predict adversary best responses
 *
 * 2. Coalition-Based Defense:
 *    - Form defensive coalitions against threats
 *    - Game-theoretic intrusion detection
 *    - Optimal resource allocation for security
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |              SECURITY-GAME THEORY BRIDGE                         |
 * +==================================================================+
 * |                                                                  |
 * |  +------------------------------------------------------------+  |
 * |  |          SECURITY -> GAME THEORY PATHWAYS                  |  |
 * |  |                                                            |  |
 * |  |  +------------------+                                      |  |
 * |  |  | PAYOFF VALIDATOR | --> Matrix integrity checks          |  |
 * |  |  | - NaN/Inf check  |     - Bound validation               |  |
 * |  |  | - Tampering det  |     - Hash verification              |  |
 * |  |  +------------------+                                      |  |
 * |  |                                                            |  |
 * |  |  +------------------+                                      |  |
 * |  |  | COALITION MONITOR| --> Formation tracking               |  |
 * |  |  | - Sybil detect   |     - Collusion detection            |  |
 * |  |  | - Pattern match  |     - Size limits                    |  |
 * |  |  +------------------+                                      |  |
 * |  |                                                            |  |
 * |  |  +------------------+                                      |  |
 * |  |  | MECHANISM VERIFY | --> Rule enforcement                 |  |
 * |  |  | - IC check       |     - IR check                       |  |
 * |  |  | - Rule integrity |     - Payment bounds                 |  |
 * |  |  +------------------+                                      |  |
 * |  |                                                            |  |
 * |  |  +------------------+                                      |  |
 * |  |  | EQUILIBRIUM AUDIT| --> Nash verification                |  |
 * |  |  | - Best response  |     - Regret bounds                  |  |
 * |  |  | - Convergence    |     - Stability check                |  |
 * |  |  +------------------+                                      |  |
 * |  +------------------------------------------------------------+  |
 * |                                                                  |
 * |  +------------------------------------------------------------+  |
 * |  |          GAME THEORY -> SECURITY PATHWAYS                  |  |
 * |  |                                                            |  |
 * |  |  +------------------+                                      |  |
 * |  |  | THREAT MODELER   | --> Strategic threat analysis        |  |
 * |  |  | - Game formulate |     - Optimal defense                |  |
 * |  |  | - Nash solve     |     - Attacker prediction            |  |
 * |  |  +------------------+                                      |  |
 * |  |                                                            |  |
 * |  |  +------------------+                                      |  |
 * |  |  | DEFENSE COALITION| --> Coalition-based defense          |  |
 * |  |  | - Formation      |     - Resource allocation            |  |
 * |  |  | - Coordination   |     - Collective action              |  |
 * |  |  +------------------+                                      |  |
 * |  +------------------------------------------------------------+  |
 * |                                                                  |
 * +------------------------------------------------------------------+
 * ```
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

#ifndef NIMCP_SECURITY_GAME_THEORY_BRIDGE_H
#define NIMCP_SECURITY_GAME_THEORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"

/* Module integrations */
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/game_theory/nimcp_gt_equilibrium.h"
#include "cognitive/game_theory/nimcp_gt_coalition.h"
#include "cognitive/game_theory/nimcp_gt_mechanism.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of reason/explanation string */
#define SECURITY_GT_MAX_REASON_LEN           256

/** Maximum players tracked for coalition monitoring */
#define SECURITY_GT_MAX_PLAYERS              32

/** Maximum coalitions to monitor */
#define SECURITY_GT_MAX_COALITIONS           64

/** Maximum payoff matrix dimension */
#define SECURITY_GT_MAX_MATRIX_DIM           64

/** Maximum manipulation events to track */
#define SECURITY_GT_MAX_MANIPULATION_EVENTS  32

/** Default payoff bound for validation */
#define SECURITY_GT_DEFAULT_PAYOFF_BOUND     1e6f

/** Default epsilon for equilibrium verification */
#define SECURITY_GT_DEFAULT_EPSILON          1e-4f

/** Magic number for validation */
#define SECURITY_GT_BRIDGE_MAGIC             0x53475442  /* 'SGTB' */

/** Module ID for bio-async */
#define SECURITY_GT_MODULE_ID                0x53475400  /* 'SGT\0' */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_game_theory_bridge security_game_theory_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Payoff validation result
 */
typedef enum {
    SECURITY_PAYOFF_VALID = 0,           /**< Payoff matrix is valid */
    SECURITY_PAYOFF_INVALID_NAN,         /**< Contains NaN values */
    SECURITY_PAYOFF_INVALID_INF,         /**< Contains Inf values */
    SECURITY_PAYOFF_INVALID_BOUNDS,      /**< Values exceed bounds */
    SECURITY_PAYOFF_INVALID_TAMPERING,   /**< Tampering detected (hash mismatch) */
    SECURITY_PAYOFF_INVALID_DIMENSION,   /**< Invalid matrix dimensions */
    SECURITY_PAYOFF_INVALID_NULL,        /**< Null pointer */
    SECURITY_PAYOFF_INVALID_ASYMMETRIC,  /**< Unexpected asymmetry */
    SECURITY_PAYOFF_INVALID_UNKNOWN      /**< Unknown validation error */
} security_payoff_status_t;

/**
 * @brief Coalition monitoring alert type
 */
typedef enum {
    SECURITY_COALITION_NORMAL = 0,       /**< Normal coalition activity */
    SECURITY_COALITION_SYBIL_DETECTED,   /**< Sybil attack detected */
    SECURITY_COALITION_COLLUSION,        /**< Collusion pattern detected */
    SECURITY_COALITION_SIZE_EXCEEDED,    /**< Coalition size limit exceeded */
    SECURITY_COALITION_RAPID_FORMATION,  /**< Unusually rapid formation */
    SECURITY_COALITION_SUSPICIOUS_SPLIT, /**< Suspicious split pattern */
    SECURITY_COALITION_BLOCKING_ABUSE,   /**< Blocking coalition abuse */
    SECURITY_COALITION_UNKNOWN           /**< Unknown alert type */
} security_coalition_alert_t;

/**
 * @brief Mechanism integrity status
 */
typedef enum {
    SECURITY_MECHANISM_VALID = 0,        /**< Mechanism is valid */
    SECURITY_MECHANISM_IC_VIOLATION,     /**< Incentive compatibility violated */
    SECURITY_MECHANISM_IR_VIOLATION,     /**< Individual rationality violated */
    SECURITY_MECHANISM_RULE_TAMPERING,   /**< Allocation/payment rule tampered */
    SECURITY_MECHANISM_TYPE_MANIPULATION,/**< Type space manipulation */
    SECURITY_MECHANISM_PAYMENT_OVERFLOW, /**< Payment calculation overflow */
    SECURITY_MECHANISM_INVALID_STATE,    /**< Invalid mechanism state */
    SECURITY_MECHANISM_UNKNOWN           /**< Unknown mechanism error */
} security_mechanism_status_t;

/**
 * @brief Equilibrium verification status
 */
typedef enum {
    SECURITY_EQUILIBRIUM_VALID = 0,      /**< Equilibrium is valid Nash */
    SECURITY_EQUILIBRIUM_INVALID_BR,     /**< Not best response for some player */
    SECURITY_EQUILIBRIUM_HIGH_REGRET,    /**< Regret exceeds threshold */
    SECURITY_EQUILIBRIUM_UNSTABLE,       /**< Equilibrium is unstable */
    SECURITY_EQUILIBRIUM_ADVERSARIAL,    /**< Adversarially crafted */
    SECURITY_EQUILIBRIUM_CONVERGENCE,    /**< Convergence issues */
    SECURITY_EQUILIBRIUM_UNKNOWN         /**< Unknown verification error */
} security_equilibrium_status_t;

/**
 * @brief Strategy manipulation type
 */
typedef enum {
    SECURITY_MANIPULATION_NONE = 0,      /**< No manipulation detected */
    SECURITY_MANIPULATION_PAYOFF_SHIFT,  /**< Payoff values shifted */
    SECURITY_MANIPULATION_STRATEGY_INJECT,/**< Strategy injection */
    SECURITY_MANIPULATION_TIMING,        /**< Timing attack on equilibrium */
    SECURITY_MANIPULATION_COALITION,     /**< Coalition-based manipulation */
    SECURITY_MANIPULATION_TYPE_MISREPORT,/**< Type misreporting in mechanism */
    SECURITY_MANIPULATION_UNKNOWN        /**< Unknown manipulation type */
} security_manipulation_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Configuration for Security-Game Theory bridge
 */
typedef struct {
    /* Payoff Validation */
    bool enable_payoff_validation;       /**< Enable payoff matrix validation */
    float payoff_lower_bound;            /**< Minimum allowed payoff value */
    float payoff_upper_bound;            /**< Maximum allowed payoff value */
    bool enable_tampering_detection;     /**< Enable hash-based tampering detection */
    bool check_nan_inf;                  /**< Check for NaN/Inf values */

    /* Coalition Monitoring */
    bool enable_coalition_monitoring;    /**< Enable coalition monitoring */
    uint32_t max_coalition_size;         /**< Maximum allowed coalition size */
    float sybil_detection_threshold;     /**< Threshold for Sybil detection */
    float collusion_threshold;           /**< Threshold for collusion detection */
    uint32_t coalition_formation_window_ms; /**< Time window for formation tracking */

    /* Mechanism Integrity */
    bool enable_mechanism_verification;  /**< Enable mechanism integrity checks */
    bool verify_incentive_compatibility; /**< Verify IC property */
    bool verify_individual_rationality;  /**< Verify IR property */
    float payment_bound;                 /**< Maximum allowed payment magnitude */

    /* Equilibrium Verification */
    bool enable_equilibrium_verification;/**< Enable equilibrium verification */
    float nash_epsilon;                  /**< Epsilon for epsilon-Nash check */
    float regret_threshold;              /**< Maximum allowed regret */
    uint32_t max_verification_iterations;/**< Max iterations for verification */

    /* Manipulation Detection */
    bool enable_manipulation_detection;  /**< Enable manipulation detection */
    float manipulation_sensitivity;      /**< Detection sensitivity [0.0-1.0] */
    uint32_t history_depth;              /**< Depth of history for pattern matching */

    /* Sensitivity Factors */
    float security_sensitivity;          /**< Security effect scaling [0.5-2.0] */
    float game_theory_sensitivity;       /**< Game theory effect scaling [0.5-2.0] */
} security_game_theory_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Payoff validation result
 */
typedef struct {
    bool is_valid;                       /**< Overall validation result */
    security_payoff_status_t status;     /**< Detailed status code */
    char reason[SECURITY_GT_MAX_REASON_LEN]; /**< Human-readable reason */

    /* Validation details */
    uint32_t nan_count;                  /**< Number of NaN values found */
    uint32_t inf_count;                  /**< Number of Inf values found */
    uint32_t out_of_bounds_count;        /**< Values outside bounds */
    float min_value;                     /**< Minimum value in matrix */
    float max_value;                     /**< Maximum value in matrix */

    /* Matrix info */
    uint32_t rows;                       /**< Matrix rows */
    uint32_t cols;                       /**< Matrix columns */
    uint32_t total_elements;             /**< Total elements checked */

    /* Timing */
    uint64_t validation_time_ns;         /**< Time to validate */
} security_payoff_result_t;

/**
 * @brief Coalition monitoring result
 */
typedef struct {
    security_coalition_alert_t alert;    /**< Alert type */
    bool is_suspicious;                  /**< Whether activity is suspicious */
    char reason[SECURITY_GT_MAX_REASON_LEN]; /**< Human-readable reason */

    /* Coalition details */
    uint32_t coalition_id;               /**< Coalition identifier */
    uint32_t coalition_size;             /**< Current coalition size */
    uint32_t members[SECURITY_GT_MAX_PLAYERS]; /**< Member IDs */
    uint32_t num_members;                /**< Number of members */

    /* Detection metrics */
    float sybil_score;                   /**< Sybil likelihood [0-1] */
    float collusion_score;               /**< Collusion likelihood [0-1] */
    float formation_rate;                /**< Formation rate (joins/second) */

    /* Timing */
    uint64_t detection_time_ns;          /**< Detection time */
    uint64_t formation_timestamp;        /**< When coalition formed */
} security_coalition_result_t;

/**
 * @brief Mechanism verification result
 */
typedef struct {
    bool is_valid;                       /**< Overall mechanism validity */
    security_mechanism_status_t status;  /**< Detailed status code */
    char reason[SECURITY_GT_MAX_REASON_LEN]; /**< Human-readable reason */

    /* IC/IR verification */
    bool incentive_compatible;           /**< IC property holds */
    bool individually_rational;          /**< IR property holds */
    float max_ic_violation;              /**< Maximum IC violation */
    float max_ir_violation;              /**< Maximum IR violation */

    /* Violating player info */
    uint32_t violating_player;           /**< Player with violation */
    uint32_t violating_type;             /**< Type with violation */
    uint32_t profitable_deviation;       /**< Profitable deviation type */

    /* Payment validation */
    float min_payment;                   /**< Minimum payment observed */
    float max_payment;                   /**< Maximum payment observed */
    bool payment_overflow_risk;          /**< Risk of payment overflow */

    /* Timing */
    uint64_t verification_time_ns;       /**< Verification time */
} security_mechanism_result_t;

/**
 * @brief Equilibrium verification result
 */
typedef struct {
    bool is_valid;                       /**< Equilibrium is valid */
    security_equilibrium_status_t status;/**< Detailed status code */
    char reason[SECURITY_GT_MAX_REASON_LEN]; /**< Human-readable reason */

    /* Nash verification */
    bool is_nash;                        /**< Is Nash equilibrium */
    float epsilon;                       /**< Epsilon for epsilon-Nash */
    float max_regret;                    /**< Maximum regret across players */
    float avg_regret;                    /**< Average regret */

    /* Per-player regrets */
    float player_regrets[SECURITY_GT_MAX_PLAYERS]; /**< Regret per player */
    uint32_t num_players;                /**< Number of players */

    /* Stability analysis */
    bool is_stable;                      /**< Equilibrium is stable */
    float stability_margin;              /**< Margin of stability */

    /* Timing */
    uint64_t verification_time_ns;       /**< Verification time */
    uint32_t iterations_used;            /**< Iterations for verification */
} security_equilibrium_result_t;

/**
 * @brief Manipulation detection result
 */
typedef struct {
    bool manipulation_detected;          /**< Whether manipulation detected */
    security_manipulation_type_t type;   /**< Type of manipulation */
    char reason[SECURITY_GT_MAX_REASON_LEN]; /**< Human-readable reason */

    /* Detection details */
    float confidence;                    /**< Detection confidence [0-1] */
    uint32_t affected_player;            /**< Player affected/involved */
    uint32_t event_count;                /**< Number of suspicious events */

    /* Evidence */
    float payoff_shift_magnitude;        /**< Magnitude of payoff shift */
    float timing_anomaly_score;          /**< Timing anomaly score */
    float pattern_match_score;           /**< Pattern matching score */

    /* Timing */
    uint64_t detection_time_ns;          /**< Detection time */
    uint64_t first_event_timestamp;      /**< First suspicious event */
} security_manipulation_result_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief Security effects on game theory
 */
typedef struct {
    /* Validation effects */
    uint32_t payoff_validations;         /**< Payoff validations performed */
    uint32_t payoff_rejections;          /**< Payoff matrices rejected */
    bool payoff_validation_active;       /**< Validation currently active */

    /* Coalition constraints */
    uint32_t coalitions_blocked;         /**< Coalitions blocked this cycle */
    uint32_t sybil_detections;           /**< Sybil attacks detected */
    bool coalition_monitoring_active;    /**< Monitoring active */

    /* Mechanism constraints */
    uint32_t mechanisms_invalidated;     /**< Mechanisms invalidated */
    bool mechanism_verification_active;  /**< Verification active */

    /* Equilibrium constraints */
    uint32_t equilibria_rejected;        /**< Equilibria rejected */
    bool equilibrium_audit_active;       /**< Audit active */

    /* Overall security state */
    float threat_level;                  /**< Current threat level [0-1] */
    bool high_security_mode;             /**< High security mode active */
} security_to_game_theory_effects_t;

/**
 * @brief Game theory effects on security
 */
typedef struct {
    /* Strategic analysis */
    uint32_t threat_games_analyzed;      /**< Threat games analyzed */
    float attacker_predicted_payoff;     /**< Predicted attacker payoff */
    float defender_optimal_payoff;       /**< Optimal defender payoff */

    /* Defense strategies */
    uint32_t defense_strategies_computed;/**< Defense strategies computed */
    float defense_effectiveness;         /**< Defense effectiveness [0-1] */
    uint32_t recommended_action;         /**< Recommended security action */

    /* Coalition defense */
    uint32_t defense_coalitions_formed;  /**< Defense coalitions formed */
    float coalition_strength;            /**< Coalition strength [0-1] */

    /* Resource allocation */
    float security_resource_allocation;  /**< Optimal resource allocation [0-1] */
    float expected_loss_reduction;       /**< Expected loss reduction [0-1] */
} game_theory_to_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current state of Security-Game Theory interaction
 */
typedef struct {
    /* Validation state */
    uint32_t pending_validations;        /**< Validations in progress */
    bool last_payoff_valid;              /**< Last payoff validation result */

    /* Coalition state */
    uint32_t active_coalitions;          /**< Currently active coalitions */
    uint32_t suspicious_coalitions;      /**< Coalitions under suspicion */
    bool sybil_alert_active;             /**< Sybil alert is active */

    /* Mechanism state */
    uint32_t active_mechanisms;          /**< Active mechanisms being monitored */
    uint32_t mechanisms_with_violations; /**< Mechanisms with IC/IR violations */

    /* Equilibrium state */
    uint32_t equilibria_verified;        /**< Equilibria verified this session */
    uint32_t equilibria_rejected;        /**< Equilibria rejected this session */

    /* Manipulation state */
    uint32_t manipulation_events;        /**< Manipulation events detected */
    float current_manipulation_risk;     /**< Current manipulation risk [0-1] */

    /* Timestamps */
    uint64_t last_validation_time;       /**< Last validation timestamp */
    uint64_t last_coalition_check_time;  /**< Last coalition check timestamp */
    uint64_t last_update_time;           /**< Last bridge update timestamp */
} security_game_theory_state_t;

/**
 * @brief Statistics for Security-Game Theory bridge
 */
typedef struct {
    /* Payoff validation statistics */
    uint64_t total_payoff_validations;   /**< Total validations performed */
    uint64_t payoff_valid_count;         /**< Valid payoff matrices */
    uint64_t payoff_invalid_nan_count;   /**< Rejected for NaN */
    uint64_t payoff_invalid_inf_count;   /**< Rejected for Inf */
    uint64_t payoff_invalid_bounds_count;/**< Rejected for bounds */
    uint64_t payoff_tampering_count;     /**< Tampering detected */
    float avg_validation_time_ns;        /**< Average validation time */

    /* Coalition monitoring statistics */
    uint64_t total_coalition_checks;     /**< Total coalition checks */
    uint64_t sybil_detections;           /**< Total Sybil detections */
    uint64_t collusion_detections;       /**< Total collusion detections */
    uint64_t coalitions_blocked;         /**< Total coalitions blocked */
    float avg_coalition_check_time_ns;   /**< Average check time */

    /* Mechanism verification statistics */
    uint64_t total_mechanism_checks;     /**< Total mechanism checks */
    uint64_t ic_violations;              /**< Total IC violations */
    uint64_t ir_violations;              /**< Total IR violations */
    uint64_t mechanisms_invalidated;     /**< Total mechanisms invalidated */
    float avg_mechanism_check_time_ns;   /**< Average check time */

    /* Equilibrium verification statistics */
    uint64_t total_equilibrium_checks;   /**< Total equilibrium checks */
    uint64_t equilibria_verified;        /**< Equilibria verified as valid */
    uint64_t equilibria_rejected;        /**< Equilibria rejected */
    float avg_equilibrium_check_time_ns; /**< Average check time */
    float avg_max_regret;                /**< Average maximum regret */

    /* Manipulation detection statistics */
    uint64_t total_manipulation_checks;  /**< Total manipulation checks */
    uint64_t manipulations_detected;     /**< Total manipulations detected */
    float avg_detection_confidence;      /**< Average detection confidence */

    /* Performance */
    uint64_t bridge_updates;             /**< Total bridge updates */
    float avg_update_time_ns;            /**< Average update time */
} security_game_theory_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Game Theory bridge state
 */
struct security_game_theory_bridge {
    bridge_base_t base;                  /**< MUST be first: base bridge */

    /* Configuration */
    security_game_theory_config_t config;

    /* Connected systems */
    nimcp_gt_system_t gt_system;         /**< Game theory system */
    nimcp_coalition_game_t coalition_game;/**< Coalition game system */
    nimcp_mechanism_t mechanism;         /**< Mechanism design system */
    nimcp_equilibrium_t equilibrium;     /**< Equilibrium solver */

    /* Current effects */
    security_to_game_theory_effects_t security_effects;
    game_theory_to_security_effects_t gt_effects;

    /* State */
    security_game_theory_state_t state;

    /* Statistics */
    security_game_theory_stats_t stats;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration with sensible values
 * WHY:  Provides starting point for customization
 * HOW:  Populates config struct with defaults
 *
 * @param config Output configuration structure
 * @return 0 on success, error code on failure
 */
int security_gt_default_config(security_game_theory_config_t* config);

/**
 * @brief Create Security-Game Theory bridge
 *
 * WHAT: Creates and initializes the bridge
 * WHY:  Enables security-game theory integration
 * HOW:  Allocates memory, initializes base, sets config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
security_game_theory_bridge_t* security_gt_bridge_create(
    const security_game_theory_config_t* config
);

/**
 * @brief Destroy Security-Game Theory bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY:  Prevents memory leaks
 * HOW:  Base cleanup, frees bridge
 *
 * @param bridge Bridge instance (NULL safe)
 */
void security_gt_bridge_destroy(security_game_theory_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect game theory system
 *
 * @param bridge Bridge instance
 * @param gt_system Game theory system
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_connect_gt_system(
    security_game_theory_bridge_t* bridge,
    nimcp_gt_system_t gt_system
);

/**
 * @brief Connect coalition game system
 *
 * @param bridge Bridge instance
 * @param coalition_game Coalition game system
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_connect_coalition_game(
    security_game_theory_bridge_t* bridge,
    nimcp_coalition_game_t coalition_game
);

/**
 * @brief Connect mechanism design system
 *
 * @param bridge Bridge instance
 * @param mechanism Mechanism design system
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_connect_mechanism(
    security_game_theory_bridge_t* bridge,
    nimcp_mechanism_t mechanism
);

/**
 * @brief Connect equilibrium solver
 *
 * @param bridge Bridge instance
 * @param equilibrium Equilibrium solver
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_connect_equilibrium(
    security_game_theory_bridge_t* bridge,
    nimcp_equilibrium_t equilibrium
);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_disconnect(security_game_theory_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge instance
 * @return true if at least one system connected
 */
bool security_gt_bridge_is_connected(
    const security_game_theory_bridge_t* bridge
);

/* ============================================================================
 * Security -> Game Theory Direction
 * ============================================================================ */

/**
 * @brief Validate payoff matrix
 *
 * WHAT: Validates payoff matrix for security issues
 * WHY:  Prevent adversarial payoff manipulation
 * HOW:  Check NaN/Inf, bounds, tampering, dimensions
 *
 * @param bridge Bridge instance
 * @param payoffs Payoff matrix data (flattened)
 * @param rows Number of rows
 * @param cols Number of columns
 * @param result Output validation result
 * @return 0 on success, error code on failure
 */
int security_gt_validate_payoff_matrix(
    security_game_theory_bridge_t* bridge,
    const float* payoffs,
    uint32_t rows,
    uint32_t cols,
    security_payoff_result_t* result
);

/**
 * @brief Monitor coalition formation
 *
 * WHAT: Monitors coalition for suspicious activity
 * WHY:  Detect Sybil attacks and collusion
 * HOW:  Analyze formation patterns, member behavior, timing
 *
 * @param bridge Bridge instance
 * @param coalition Coalition bitmask
 * @param player_ids Player IDs in coalition
 * @param num_players Number of players
 * @param result Output monitoring result
 * @return 0 on success, error code on failure
 */
int security_gt_monitor_coalition(
    security_game_theory_bridge_t* bridge,
    uint32_t coalition,
    const uint32_t* player_ids,
    uint32_t num_players,
    security_coalition_result_t* result
);

/**
 * @brief Verify mechanism design integrity
 *
 * WHAT: Verifies mechanism design rules are enforced
 * WHY:  Prevent mechanism manipulation
 * HOW:  Check IC, IR, payment bounds, rule integrity
 *
 * @param bridge Bridge instance
 * @param mechanism Mechanism to verify
 * @param result Output verification result
 * @return 0 on success, error code on failure
 */
int security_gt_verify_mechanism(
    security_game_theory_bridge_t* bridge,
    nimcp_mechanism_t mechanism,
    security_mechanism_result_t* result
);

/**
 * @brief Check Nash equilibrium validity
 *
 * WHAT: Validates that strategy profile is Nash equilibrium
 * WHY:  Detect adversarially crafted "equilibria"
 * HOW:  Verify best response for all players, check regrets
 *
 * @param bridge Bridge instance
 * @param equilibrium Equilibrium solver context
 * @param strategies Strategy profile to verify
 * @param result Output verification result
 * @return 0 on success, error code on failure
 */
int security_gt_check_equilibrium(
    security_game_theory_bridge_t* bridge,
    nimcp_equilibrium_t equilibrium,
    const nimcp_strategy_profile_t* strategies,
    security_equilibrium_result_t* result
);

/**
 * @brief Detect strategy manipulation
 *
 * WHAT: Detects manipulation attempts in game play
 * WHY:  Identify adversarial strategy manipulation
 * HOW:  Pattern matching, timing analysis, anomaly detection
 *
 * @param bridge Bridge instance
 * @param player_id Player to check
 * @param recent_actions Recent actions taken
 * @param num_actions Number of recent actions
 * @param result Output detection result
 * @return 0 on success, error code on failure
 */
int security_gt_detect_manipulation(
    security_game_theory_bridge_t* bridge,
    uint32_t player_id,
    const uint32_t* recent_actions,
    uint32_t num_actions,
    security_manipulation_result_t* result
);

/* ============================================================================
 * Game Theory -> Security Direction
 * ============================================================================ */

/**
 * @brief Analyze threat as game
 *
 * WHAT: Models security threat as game-theoretic interaction
 * WHY:  Compute optimal defense strategies
 * HOW:  Formulate attacker/defender game, find equilibrium
 *
 * @param bridge Bridge instance
 * @param attacker_payoffs Attacker payoff matrix
 * @param defender_payoffs Defender payoff matrix
 * @param num_attacker_actions Attacker action count
 * @param num_defender_actions Defender action count
 * @param optimal_defense Output: optimal defender strategy
 * @param expected_payoff Output: expected defender payoff
 * @return 0 on success, error code on failure
 */
int security_gt_analyze_threat_game(
    security_game_theory_bridge_t* bridge,
    const float* attacker_payoffs,
    const float* defender_payoffs,
    uint32_t num_attacker_actions,
    uint32_t num_defender_actions,
    float* optimal_defense,
    float* expected_payoff
);

/**
 * @brief Form defensive coalition
 *
 * WHAT: Forms coalition for collective defense
 * WHY:  Coordinate multi-agent defense
 * HOW:  Game-theoretic coalition formation for defense
 *
 * @param bridge Bridge instance
 * @param defender_ids IDs of potential defenders
 * @param num_defenders Number of potential defenders
 * @param coalition_out Output: formed coalition bitmask
 * @param strength_out Output: coalition strength
 * @return 0 on success, error code on failure
 */
int security_gt_form_defensive_coalition(
    security_game_theory_bridge_t* bridge,
    const uint32_t* defender_ids,
    uint32_t num_defenders,
    uint32_t* coalition_out,
    float* strength_out
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update bridge (bidirectional)
 *
 * WHAT: Performs periodic bridge update
 * WHY:  Synchronizes state, performs background checks
 * HOW:  Updates effects, runs periodic validations
 *
 * @param bridge Bridge instance
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_update(
    security_game_theory_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply security effects to game theory
 *
 * WHAT: Applies current security constraints to game theory
 * WHY:  Propagates security state to game-theoretic reasoning
 * HOW:  Updates blocked coalitions, validation state
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_gt_apply_security_effects(
    security_game_theory_bridge_t* bridge
);

/**
 * @brief Apply game theory effects to security
 *
 * WHAT: Applies game-theoretic analysis to security
 * WHY:  Enables strategic security decisions
 * HOW:  Updates threat models, defense recommendations
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_gt_apply_gt_effects(
    security_game_theory_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_get_state(
    const security_game_theory_bridge_t* bridge,
    security_game_theory_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_get_stats(
    const security_game_theory_bridge_t* bridge,
    security_game_theory_stats_t* stats
);

/**
 * @brief Get security effects on game theory
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_gt_get_security_effects(
    const security_game_theory_bridge_t* bridge,
    security_to_game_theory_effects_t* effects
);

/**
 * @brief Get game theory effects on security
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_gt_get_gt_effects(
    const security_game_theory_bridge_t* bridge,
    game_theory_to_security_effects_t* effects
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_reset_stats(security_game_theory_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_connect_bio_async(
    security_game_theory_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_gt_bridge_disconnect_bio_async(
    security_game_theory_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool security_gt_bridge_is_bio_async_connected(
    const security_game_theory_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_GAME_THEORY_BRIDGE_H */
