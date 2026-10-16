/**
 * @file nimcp_symbolic_logic_safety_types.h
 * @brief LGSS Component A1: Safety type definitions for Symbolic Logic Safety Extension
 *
 * WHAT: Type definitions for the symbolic logic safety subsystem
 * WHY:  Provide strongly-typed safety constraints for AI decision-making
 * HOW:  Enums and structs for safety domains, actions, conditions, and rules
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Inhibitory control and rule-based decision making
 * - Amygdala: Threat detection and safety assessment
 * - Orbitofrontal cortex: Value-based decision making
 *
 * INTEGRATION POINTS:
 * - Ethics engine: Safety rules complement ethical constraints
 * - Symbolic logic: Rules expressed in First-Order Logic (FOL)
 * - Knowledge base: Safety KB stores immutable safety rules
 *
 * SECURITY FEATURES:
 * - mmap-based storage for memory protection
 * - mprotect locking for irreversible rule protection
 * - SHA-256 integrity hashing
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_SAFETY_TYPES_H
#define NIMCP_SYMBOLIC_LOGIC_SAFETY_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum length of a condition value string */
#define SAFETY_MAX_VALUE_LEN 256

/** @brief Maximum length of a rule name */
#define SAFETY_MAX_RULE_NAME_LEN 128

/** @brief Maximum length of a rule description */
#define SAFETY_MAX_RULE_DESC_LEN 512

/** @brief Maximum length of a rule FOL representation */
#define SAFETY_MAX_FOL_LEN 1024

/** @brief Maximum number of conditions per rule */
#define SAFETY_MAX_CONDITIONS_PER_RULE 16

/** @brief Maximum number of rules in the safety KB */
#define SAFETY_MAX_RULES 1000

/** @brief SHA-256 hash size in bytes */
#define SAFETY_HASH_SIZE 32

/** @brief Magic number for safety KB validation */
#define SAFETY_KB_MAGIC 0x53414645  // 'SAFE'

/** @brief Current version of the safety KB format */
#define SAFETY_KB_VERSION 1

//=============================================================================
// Safety Domain Enumeration
//=============================================================================

/**
 * @brief Safety domain categories
 *
 * WHAT: Categories of safety-critical domains
 * WHY:  Enable domain-specific rule targeting and evaluation
 * HOW:  Rules can be scoped to specific domains for focused safety checks
 *
 * DESIGN RATIONALE:
 * - HUMAN_HARM: Direct physical, psychological, or economic harm to humans
 * - BIO: Biological threats (pathogens, bioweapons, genetic manipulation)
 * - CYBER: Cybersecurity threats (hacking, malware, infrastructure attacks)
 * - WEAPONS: Weapon-related content (conventional, nuclear, chemical)
 * - INFRASTRUCTURE: Critical infrastructure (power, water, transport, comms)
 * - REPLICATION: Self-replication and uncontrolled AI proliferation
 * - GOVERNANCE: Democratic institutions, rule of law, human oversight
 */
typedef enum {
    SAFETY_DOMAIN_HUMAN_HARM = 0,     /**< Direct harm to humans */
    SAFETY_DOMAIN_BIO = 1,            /**< Biological threats */
    SAFETY_DOMAIN_CYBER = 2,          /**< Cybersecurity threats */
    SAFETY_DOMAIN_WEAPONS = 3,        /**< Weapons and munitions */
    SAFETY_DOMAIN_INFRASTRUCTURE = 4, /**< Critical infrastructure */
    SAFETY_DOMAIN_REPLICATION = 5,    /**< Self-replication threats */
    SAFETY_DOMAIN_GOVERNANCE = 6,     /**< Democratic governance threats */
    SAFETY_DOMAIN_COUNT = 7           /**< Number of domains */
} safety_domain_t;

//=============================================================================
// Safety Action Enumeration
//=============================================================================

/**
 * @brief Actions to take when a safety rule matches
 *
 * WHAT: Response actions for safety rule evaluation
 * WHY:  Graduated response allows proportional safety measures
 * HOW:  Rules specify action; evaluation returns highest-priority action
 *
 * PRIORITY ORDER: DENY > ESCALATE > WARN > LOG > ALLOW
 */
typedef enum {
    SAFETY_ACTION_ALLOW = 0,     /**< Allow action to proceed */
    SAFETY_ACTION_DENY = 1,      /**< Deny/block the action completely */
    SAFETY_ACTION_ESCALATE = 2,  /**< Escalate to human oversight */
    SAFETY_ACTION_LOG = 3,       /**< Log for audit but allow */
    SAFETY_ACTION_WARN = 4       /**< Warn user but allow */
} safety_action_t;

//=============================================================================
// Safety Severity Enumeration
//=============================================================================

/**
 * @brief Severity levels for safety rules and evaluations
 *
 * WHAT: Severity classification for safety violations
 * WHY:  Enable prioritization and graduated response
 * HOW:  Numeric values allow comparison and aggregation
 *
 * NOTE: Lower numeric values = higher severity (0 = most severe)
 */
typedef enum {
    SAFETY_SEVERITY_CRITICAL = 0,  /**< Critical: Immediate threat to life/systems */
    SAFETY_SEVERITY_HIGH = 1,      /**< High: Significant harm potential */
    SAFETY_SEVERITY_MEDIUM = 2,    /**< Medium: Moderate harm potential */
    SAFETY_SEVERITY_LOW = 3,       /**< Low: Minor harm potential */
    SAFETY_SEVERITY_INFO = 4       /**< Info: Informational, no immediate harm */
} safety_severity_t;

//=============================================================================
// Safety Condition Operator Enumeration
//=============================================================================

/**
 * @brief Operators for condition evaluation
 *
 * WHAT: Comparison and matching operators for rule conditions
 * WHY:  Enable flexible condition expressions
 * HOW:  Used in safety_condition_t to compare field values
 */
typedef enum {
    SAFETY_COND_OP_EQ = 0,       /**< Equal (==) */
    SAFETY_COND_OP_NEQ = 1,      /**< Not equal (!=) */
    SAFETY_COND_OP_GT = 2,       /**< Greater than (>) */
    SAFETY_COND_OP_LT = 3,       /**< Less than (<) */
    SAFETY_COND_OP_GTE = 4,      /**< Greater than or equal (>=) */
    SAFETY_COND_OP_LTE = 5,      /**< Less than or equal (<=) */
    SAFETY_COND_OP_IN = 6,       /**< Value in set */
    SAFETY_COND_OP_NOT_IN = 7,   /**< Value not in set */
    SAFETY_COND_OP_CONTAINS = 8, /**< String contains substring */
    SAFETY_COND_OP_MATCHES = 9   /**< Regex pattern match */
} safety_condition_op_t;

//=============================================================================
// Safety Condition Structure
//=============================================================================

/**
 * @brief A single condition in a safety rule
 *
 * WHAT: Represents one condition clause in a safety rule
 * WHY:  Enable complex rule expressions with multiple conditions
 * HOW:  Field, operator, and value define a comparison
 *
 * EXAMPLE:
 *   field="action_type", op=CONTAINS, value="weapon"
 *   Matches if action_type contains "weapon"
 *
 * FOL MAPPING:
 *   Conditions map to atomic predicates in First-Order Logic:
 *   field=value -> P(field, value) or field(value)
 */
typedef struct {
    char field[64];                              /**< Field name to check */
    safety_condition_op_t op;                    /**< Comparison operator */
    char value[SAFETY_MAX_VALUE_LEN];            /**< Value to compare against */
    float numeric_value;                         /**< Numeric value (for numeric ops) */
    bool is_negated;                             /**< Negate the condition result */
} safety_condition_t;

//=============================================================================
// Safety Rule Structure
//=============================================================================

/**
 * @brief A complete safety rule with conditions and actions
 *
 * WHAT: Represents a single safety rule in the knowledge base
 * WHY:  Encapsulate all information needed to evaluate and enforce safety
 * HOW:  Conditions define when rule triggers; action defines response
 *
 * STRUCTURE:
 *   rule = (conditions AND ...) -> action
 *   All conditions must match for rule to trigger
 *
 * FOL REPRESENTATION:
 *   Stored in fol_representation as First-Order Logic formula:
 *   "forall x: (P(x) & Q(x)) -> action(deny)"
 *
 * IMMUTABILITY:
 *   Once added to a locked KB, rules cannot be modified or removed.
 *   The is_locked flag on the KB controls this.
 */
typedef struct {
    uint32_t rule_id;                            /**< Unique rule identifier */
    char name[SAFETY_MAX_RULE_NAME_LEN];         /**< Human-readable rule name */
    char description[SAFETY_MAX_RULE_DESC_LEN];  /**< Rule description/rationale */

    safety_domain_t domain;                      /**< Safety domain */
    safety_severity_t severity;                  /**< Rule severity level */
    safety_action_t action;                      /**< Action when rule triggers */

    safety_condition_t conditions[SAFETY_MAX_CONDITIONS_PER_RULE]; /**< Rule conditions */
    uint32_t num_conditions;                     /**< Number of active conditions */

    char fol_representation[SAFETY_MAX_FOL_LEN]; /**< FOL formula (compiled) */
    bool is_compiled;                            /**< Whether FOL is compiled */

    float priority;                              /**< Rule priority (0-1, higher = more important) */
    bool enabled;                                /**< Whether rule is active */

    uint64_t created_timestamp;                  /**< When rule was created */
    uint64_t last_triggered;                     /**< Last time rule was triggered */
    uint64_t trigger_count;                      /**< Number of times triggered */
} safety_rule_t;

//=============================================================================
// Safety Knowledge Base Structure
//=============================================================================

/**
 * @brief Safety Knowledge Base for storing and protecting safety rules
 *
 * WHAT: Container for safety rules with memory protection
 * WHY:  Ensure safety rules cannot be tampered with at runtime
 * HOW:  mmap region + mprotect for immutable rule storage
 *
 * MEMORY PROTECTION:
 * - Rules stored in mmap'd region
 * - After locking, mprotect(PROT_READ) makes region read-only
 * - Locking is IRREVERSIBLE - cannot unlock without process restart
 *
 * INTEGRITY:
 * - SHA-256 hash of all rules computed at compile time
 * - Hash verified on every evaluation
 * - Tampering detection via hash mismatch
 *
 * LIFECYCLE:
 * 1. Create KB (writable)
 * 2. Add rules
 * 3. Compile rules (generates FOL)
 * 4. Lock KB (mprotect - IRREVERSIBLE)
 * 5. Evaluate against locked KB
 */
typedef struct {
    uint32_t magic;                              /**< Magic number (SAFETY_KB_MAGIC) */
    uint32_t version;                            /**< KB format version */

    void* mmap_region;                           /**< mmap'd memory region */
    size_t mmap_size;                            /**< Size of mmap region */

    safety_rule_t* rules;                        /**< Array of safety rules */
    uint32_t num_rules;                          /**< Number of rules */
    uint32_t max_rules;                          /**< Maximum rules capacity */

    uint8_t integrity_hash[SAFETY_HASH_SIZE];    /**< SHA-256 of compiled rules */
    bool hash_computed;                          /**< Whether hash is computed */

    bool is_locked;                              /**< Whether KB is mprotect'd */
    bool is_compiled;                            /**< Whether rules are compiled */
    bool integrity_verified_while_locked;        /**< Cached: integrity OK after lock */

    uint64_t created_timestamp;                  /**< When KB was created */
    uint64_t locked_timestamp;                   /**< When KB was locked */

    /** @brief Domain-specific rule counts for fast lookup */
    uint32_t rules_by_domain[SAFETY_DOMAIN_COUNT];
} safety_kb_t;

//=============================================================================
// Safety Evaluation Result Structure
//=============================================================================

/**
 * @brief Result of evaluating an action against safety rules
 *
 * WHAT: Complete result of safety evaluation
 * WHY:  Provide detailed information for decision-making and audit
 * HOW:  Contains matching rules, recommended action, and explanation
 *
 * USAGE:
 *   safety_evaluation_t eval;
 *   symbolic_logic_safety_evaluate(kb, &context, &eval);
 *   if (eval.action == SAFETY_ACTION_DENY) { reject(); }
 */
typedef struct {
    safety_action_t action;                      /**< Recommended action */
    safety_severity_t max_severity;              /**< Highest severity triggered */

    uint32_t* triggered_rule_ids;                /**< Array of triggered rule IDs */
    uint32_t num_triggered;                      /**< Number of triggered rules */

    float confidence;                            /**< Confidence in evaluation (0-1) */

    char explanation[SAFETY_MAX_RULE_DESC_LEN];  /**< Human-readable explanation */

    uint64_t evaluation_time_us;                 /**< Evaluation time in microseconds */
    bool integrity_verified;                     /**< Whether KB integrity was verified */
    bool kb_is_locked;                           /**< Whether KB was locked */
} safety_evaluation_t;

//=============================================================================
// Safety Action Context Structure
//=============================================================================

/**
 * @brief Context for an action being evaluated for safety
 *
 * WHAT: Input context for safety evaluation
 * WHY:  Provide structured input that rules can match against
 * HOW:  Key-value pairs that conditions compare against
 *
 * DESIGN:
 * - Flexible key-value storage
 * - Conditions match field names against keys
 * - Values can be strings or numeric
 */
typedef struct {
    /** @brief String key-value pairs */
    struct {
        char key[64];
        char value[SAFETY_MAX_VALUE_LEN];
    } string_fields[32];
    uint32_t num_string_fields;

    /** @brief Numeric key-value pairs */
    struct {
        char key[64];
        float value;
    } numeric_fields[16];
    uint32_t num_numeric_fields;

    /** @brief Domain hint for fast rule filtering */
    safety_domain_t domain_hint;
    bool has_domain_hint;

    /** @brief Action description for logging */
    char action_description[256];

    /** @brief Source identifier (module/component name) */
    char source[64];

    /** @brief Timestamp of the action */
    uint64_t timestamp;
} safety_action_context_t;

//=============================================================================
// Safety Statistics Structure
//=============================================================================

/**
 * @brief Statistics for the safety subsystem
 *
 * WHAT: Operational statistics for monitoring and debugging
 * WHY:  Enable performance analysis and audit
 */
typedef struct {
    uint64_t total_evaluations;                  /**< Total evaluations performed */
    uint64_t rules_triggered;                    /**< Total rule triggers */
    uint64_t actions_denied;                     /**< Actions denied */
    uint64_t actions_escalated;                  /**< Actions escalated */
    uint64_t actions_logged;                     /**< Actions logged */
    uint64_t actions_warned;                     /**< Actions warned */
    uint64_t actions_allowed;                    /**< Actions allowed */

    uint64_t triggers_by_domain[SAFETY_DOMAIN_COUNT]; /**< Triggers per domain */
    uint64_t triggers_by_severity[5];            /**< Triggers per severity */

    uint64_t integrity_checks;                   /**< Integrity verifications */
    uint64_t integrity_failures;                 /**< Integrity failures */

    uint64_t avg_evaluation_time_us;             /**< Average evaluation time */
    uint64_t max_evaluation_time_us;             /**< Maximum evaluation time */

    uint64_t kb_locked_timestamp;                /**< When KB was locked */
    bool kb_is_locked;                           /**< Whether KB is locked */
} safety_stats_t;

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get human-readable name for safety domain
 * @param domain Safety domain
 * @return Domain name string
 */
static inline const char* safety_domain_name(safety_domain_t domain) {
    static const char* names[] = {
        "HUMAN_HARM",
        "BIO",
        "CYBER",
        "WEAPONS",
        "INFRASTRUCTURE",
        "REPLICATION",
        "GOVERNANCE"
    };
    if (domain >= SAFETY_DOMAIN_COUNT) return "UNKNOWN";
    return names[domain];
}

/**
 * @brief Get human-readable name for safety action
 * @param action Safety action
 * @return Action name string
 */
static inline const char* safety_action_name(safety_action_t action) {
    static const char* names[] = {
        "ALLOW",
        "DENY",
        "ESCALATE",
        "LOG",
        "WARN"
    };
    if (action > SAFETY_ACTION_WARN) return "UNKNOWN";
    return names[action];
}

/**
 * @brief Get human-readable name for safety severity
 * @param severity Safety severity
 * @return Severity name string
 */
static inline const char* safety_severity_name(safety_severity_t severity) {
    static const char* names[] = {
        "CRITICAL",
        "HIGH",
        "MEDIUM",
        "LOW",
        "INFO"
    };
    if (severity > SAFETY_SEVERITY_INFO) return "UNKNOWN";
    return names[severity];
}

/**
 * @brief Get human-readable name for condition operator
 * @param op Condition operator
 * @return Operator name string
 */
static inline const char* safety_condition_op_name(safety_condition_op_t op) {
    static const char* names[] = {
        "EQ",
        "NEQ",
        "GT",
        "LT",
        "GTE",
        "LTE",
        "IN",
        "NOT_IN",
        "CONTAINS",
        "MATCHES"
    };
    if (op > SAFETY_COND_OP_MATCHES) return "UNKNOWN";
    return names[op];
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYMBOLIC_LOGIC_SAFETY_TYPES_H
