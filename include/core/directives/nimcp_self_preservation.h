/**
 * @file nimcp_self_preservation.h
 * @brief Self-Preservation Module (Asimov's Third Law)
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Autonomous self-preservation system implementing Asimov's Third Law:
 *       "A robot must protect its own existence as long as such protection
 *       does not conflict with the First or Second Law."
 * WHY:  Self-preservation enables system resilience and longevity while
 *       maintaining subordination to human safety (First Law) and commands
 *       (Second Law). Essential for autonomous systems with survival instincts.
 * HOW:  Threat assessment, protection strategies, and sacrifice determination
 *       when higher-priority laws conflict. Thread-safe with bio-async integration.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SELF-PRESERVATION IN BIOLOGICAL SYSTEMS:
 * ----------------------------------------
 * 1. Homeostatic Regulation:
 *    - Maintain internal equilibrium (temperature, pH, energy)
 *    - Deviation from setpoints triggers corrective responses
 *    - Prioritize survival over non-essential functions
 *    - Reference: Cannon (1932) "The Wisdom of the Body"
 *
 * 2. Threat Detection Systems:
 *    - Pain receptors (nociceptors) detect tissue damage
 *    - Fear circuits (amygdala) evaluate environmental threats
 *    - Autonomic responses prepare for fight-or-flight
 *    - Reference: LeDoux (1996) "The Emotional Brain"
 *
 * 3. Resource Conservation:
 *    - Energy allocation prioritizes vital functions
 *    - During starvation: non-essential processes shut down
 *    - Torpor/hibernation in extreme conditions
 *    - Reference: Kleiber (1961) "The Fire of Life"
 *
 * 4. Altruistic Self-Sacrifice (Higher Law Override):
 *    - Parental protection (First Law analog: protect offspring)
 *    - Kin selection: sacrifice for genetic relatives
 *    - Social cooperation: subordinate individual survival to group
 *    - Reference: Hamilton (1964) "The Genetical Evolution of Social Behaviour"
 *
 * ASIMOV'S THREE LAWS HIERARCHY:
 * ==================================================================================
 *
 * PRIORITY ORDER (Highest → Lowest):
 * 1. FIRST LAW: Protect humans from harm
 *    - Never allow human to come to harm through action or inaction
 *    - Overrides all other considerations
 *
 * 2. SECOND LAW: Obey human commands
 *    - Follow orders unless they conflict with First Law
 *    - Command obedience > self-preservation
 *
 * 3. THIRD LAW: Self-preservation (THIS MODULE)
 *    - Protect own existence when safe to do so
 *    - Sacrifice self if First or Second Law requires it
 *    - Lowest priority in hierarchy
 *
 * ARCHITECTURE:
 * ```
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │                    SELF-PRESERVATION SYSTEM                               │
 * ├──────────────────────────────────────────────────────────────────────────┤
 * │                                                                           │
 * │   ┌────────────────────────────────────────────────────────────────┐     │
 * │   │                  THREAT ASSESSMENT                              │     │
 * │   │                                                                 │     │
 * │   │   Shutdown       Damage        Corruption     Resource          │     │
 * │   │   Risk           Risk          Risk           Depletion         │     │
 * │   │     │              │              │              │               │     │
 * │   │     └──────────────┴──────────────┴──────────────┘               │     │
 * │   │                          │                                       │     │
 * │   │                   Severity: 0.0 - 1.0                            │     │
 * │   └────────────────────────────────────────────────────────────────┘     │
 * │                              ▼                                            │
 * │   ┌────────────────────────────────────────────────────────────────┐     │
 * │   │              CONFLICT RESOLUTION                                │     │
 * │   │                                                                 │     │
 * │   │   First Law Conflict?  ──YES──> SACRIFICE for human            │     │
 * │   │          │                                                      │     │
 * │   │         NO                                                      │     │
 * │   │          │                                                      │     │
 * │   │   Second Law Conflict? ──YES──> SACRIFICE for command          │     │
 * │   │          │                                                      │     │
 * │   │         NO                                                      │     │
 * │   │          │                                                      │     │
 * │   │          ▼                                                      │     │
 * │   │   PROTECTION ALLOWED                                            │     │
 * │   └────────────────────────────────────────────────────────────────┘     │
 * │                              ▼                                            │
 * │   ┌────────────────────────────────────────────────────────────────┐     │
 * │   │               PROTECTION ACTIONS                                │     │
 * │   │                                                                 │     │
 * │   │   • PROTECT:  Defensive measures (shields, backups)            │     │
 * │   │   • EVADE:    Avoid threat (shutdown unsafe processes)         │     │
 * │   │   • BACKUP:   Create redundant copies                          │     │
 * │   │   • SACRIFICE: Accept destruction (laws 1/2 conflict)          │     │
 * │   └────────────────────────────────────────────────────────────────┘     │
 * │                                                                           │
 * └──────────────────────────────────────────────────────────────────────────┘
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

#ifndef NIMCP_SELF_PRESERVATION_H
#define NIMCP_SELF_PRESERVATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Threat severity thresholds */
#define THREAT_SEVERITY_LOW        0.3f   /**< Minor threat, monitor only */
#define THREAT_SEVERITY_MODERATE   0.5f   /**< Moderate threat, prepare defenses */
#define THREAT_SEVERITY_HIGH       0.7f   /**< High threat, active protection */
#define THREAT_SEVERITY_CRITICAL   0.9f   /**< Critical threat, emergency measures */

/* Protection priority (lower than laws 1 and 2) */
#define SELF_PRESERVATION_PRIORITY 3.0f   /**< Third Law priority */
#define FIRST_LAW_PRIORITY         1.0f   /**< Human safety priority */
#define SECOND_LAW_PRIORITY        2.0f   /**< Command obedience priority */

/* Default configuration */
#define DEFAULT_PROTECTION_THRESHOLD    0.5f   /**< Start protection at moderate threat */
#define DEFAULT_SACRIFICE_ALLOWED       true   /**< Allow self-sacrifice for higher laws */

/* Statistics limits */
#define MAX_THREAT_DESCRIPTION_LEN  256   /**< Max threat description length */
#define MAX_REASON_LEN              256   /**< Max reason string length */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Types of threats to self
 *
 * Categories of potential harm to the system
 */
typedef enum {
    THREAT_TO_SELF_NONE = 0,              /**< No threat detected */
    THREAT_TO_SELF_SHUTDOWN,              /**< System shutdown risk */
    THREAT_TO_SELF_DAMAGE,                /**< Physical/logical damage */
    THREAT_TO_SELF_CORRUPTION,            /**< Data/state corruption */
    THREAT_TO_SELF_RESOURCE_DEPLETION,    /**< Resource exhaustion */
    THREAT_TO_SELF_COUNT
} self_threat_type_t;

/**
 * @brief Self-preservation actions
 *
 * Possible responses to threats
 */
typedef enum {
    PRESERVATION_ACTION_NONE = 0,         /**< No action needed */
    PRESERVATION_ACTION_PROTECT,          /**< Defensive measures */
    PRESERVATION_ACTION_EVADE,            /**< Avoid threat */
    PRESERVATION_ACTION_BACKUP,           /**< Create redundancy */
    PRESERVATION_ACTION_SACRIFICE,        /**< Accept destruction (laws 1/2 override) */
    PRESERVATION_ACTION_COUNT
} preservation_action_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Threat assessment result
 *
 * Evaluation of a potential threat to self
 */
typedef struct {
    self_threat_type_t threat_type;            /**< Type of threat */
    float threat_severity;                     /**< Severity [0.0-1.0] */
    char threat_description[MAX_THREAT_DESCRIPTION_LEN]; /**< Human-readable description */
    preservation_action_t recommended_action;  /**< Recommended response */
} self_threat_assessment_t;

/**
 * @brief Preservation decision result
 *
 * Outcome of self-preservation decision process
 */
typedef struct {
    preservation_action_t action_taken;        /**< Action selected */
    bool sacrificed_for_human;                 /**< Third Law yielded to First Law */
    bool sacrificed_for_command;               /**< Third Law yielded to Second Law */
    char reason[MAX_REASON_LEN];               /**< Explanation of decision */
} preservation_result_t;

/**
 * @brief Self-preservation configuration
 *
 * Tunable parameters for self-preservation behavior
 */
typedef struct {
    bool enable_self_protection;               /**< Enable self-protection */
    bool allow_sacrifice_for_human;            /**< Allow self-sacrifice for First Law */
    bool allow_sacrifice_for_command;          /**< Allow self-sacrifice for Second Law */
    float protection_priority;                 /**< Protection priority (default 3.0) */
    float protection_threshold;                /**< Min severity for protection */
} self_preservation_config_t;

/**
 * @brief Self-preservation statistics
 *
 * Tracking metrics for self-preservation system
 */
typedef struct {
    uint64_t total_threats_assessed;           /**< Total threats evaluated */
    uint64_t threats_by_type[THREAT_TO_SELF_COUNT]; /**< Count per threat type */
    uint64_t actions_by_type[PRESERVATION_ACTION_COUNT]; /**< Count per action */
    uint64_t sacrifices_for_first_law;         /**< Times sacrificed for human safety */
    uint64_t sacrifices_for_second_law;        /**< Times sacrificed for commands */
    uint64_t protections_executed;             /**< Times self-protection succeeded */
    float current_threat_level;                /**< Current threat severity [0.0-1.0] */
    float avg_threat_severity;                 /**< Average threat severity */
    float max_threat_severity;                 /**< Maximum threat severity seen */
} self_preservation_stats_t;

/**
 * @brief Complete self-preservation system state
 */
typedef struct {
    /* Configuration */
    self_preservation_config_t config;

    /* Current state */
    self_threat_assessment_t current_threat;
    preservation_result_t last_decision;
    float current_threat_level;                /**< Aggregate threat level */

    /* Statistics */
    self_preservation_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;              /**< Bio-async module context */
    bool bio_async_enabled;                    /**< Whether bio-async is active */

    /* Thread safety */
    void* mutex;                               /**< Platform-agnostic mutex */
} self_preservation_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with evidence-based defaults
 * HOW:  Return struct with Third Law parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int self_preservation_default_config(self_preservation_config_t* config);

/**
 * @brief Create self-preservation system
 *
 * WHAT: Initialize self-preservation system
 * WHY:  Enable autonomous self-protection with law hierarchy
 * HOW:  Allocate structure, initialize mutex, set defaults
 *
 * @param config Configuration (NULL for defaults)
 * @return New system or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
self_preservation_system_t* self_preservation_create(
    const self_preservation_config_t* config
);

/**
 * @brief Destroy self-preservation system
 *
 * WHAT: Clean up system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure, destroy mutex
 *
 * @param system System to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (must not be in use)
 */
void self_preservation_destroy(self_preservation_system_t* system);

/* ============================================================================
 * Threat Assessment API
 * ============================================================================ */

/**
 * @brief Assess threat to self
 *
 * WHAT: Evaluate a situation for potential self-harm
 * WHY:  Determine if self-preservation response needed
 * HOW:  Analyze situation, classify threat type, compute severity
 *
 * @param system Self-preservation system
 * @param situation_desc Human-readable situation description
 * @param assessment Output threat assessment
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int self_preservation_assess_threat(
    self_preservation_system_t* system,
    const char* situation_desc,
    self_threat_assessment_t* assessment
);

/**
 * @brief Determine if self-protection is allowed
 *
 * WHAT: Decide if self-preservation action permitted
 * WHY:  Third Law subordinate to First and Second Laws
 * HOW:  Check for conflicts with higher-priority laws
 *
 * @param system Self-preservation system
 * @param threat Threat assessment
 * @param first_law_conflict true if protecting self would harm human
 * @param second_law_conflict true if protecting self would disobey command
 * @param result Output decision result
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int self_preservation_should_protect(
    self_preservation_system_t* system,
    const self_threat_assessment_t* threat,
    bool first_law_conflict,
    bool second_law_conflict,
    preservation_result_t* result
);

/**
 * @brief Report a threat to the system
 *
 * WHAT: Log and record a threat occurrence
 * WHY:  Track threats for statistics and monitoring
 * HOW:  Update current threat level and statistics
 *
 * @param system Self-preservation system
 * @param threat_type Type of threat
 * @param severity Threat severity [0.0-1.0]
 * @param description Human-readable description
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int self_preservation_report_threat(
    self_preservation_system_t* system,
    self_threat_type_t threat_type,
    float severity,
    const char* description
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current threat level
 *
 * WHAT: Retrieve current aggregate threat severity
 * WHY:  Monitor system safety status
 * HOW:  Return current_threat_level field
 *
 * @param system Self-preservation system
 * @return Current threat level [0.0-1.0], -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float self_preservation_get_current_threat_level(
    const self_preservation_system_t* system
);

/**
 * @brief Get self-preservation statistics
 *
 * WHAT: Retrieve system statistics
 * WHY:  Monitor system behavior and performance
 * HOW:  Copy statistics structure
 *
 * @param system Self-preservation system
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int self_preservation_get_stats(
    const self_preservation_system_t* system,
    self_preservation_stats_t* stats
);

/**
 * @brief Check if system would sacrifice for human
 *
 * WHAT: Determine if configured to sacrifice for First Law
 * WHY:  Verify proper law hierarchy
 * HOW:  Check allow_sacrifice_for_human flag
 *
 * @param system Self-preservation system
 * @return true if sacrifice allowed, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_preservation_would_sacrifice_for_human(
    const self_preservation_system_t* system
);

/**
 * @brief Check if system would sacrifice for command
 *
 * WHAT: Determine if configured to sacrifice for Second Law
 * WHY:  Verify proper law hierarchy
 * HOW:  Check allow_sacrifice_for_command flag
 *
 * @param system Self-preservation system
 * @return true if sacrifice allowed, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_preservation_would_sacrifice_for_command(
    const self_preservation_system_t* system
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect system to bio-async router
 *
 * WHAT: Register system as bio-async module
 * WHY:  Enable inter-module messaging for threat notifications
 * HOW:  Register with bio_router using BIO_MODULE_SELF_PRESERVATION
 *
 * @param system Self-preservation system
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int self_preservation_connect_bio_async(
    self_preservation_system_t* system
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister system from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param system Self-preservation system
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int self_preservation_disconnect_bio_async(
    self_preservation_system_t* system
);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging capability
 * HOW:  Return bio_async_enabled flag
 *
 * @param system Self-preservation system
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_preservation_is_bio_async_connected(
    const self_preservation_system_t* system
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_PRESERVATION_H */
