/**
 * @file nimcp_lgss.h
 * @brief Layered Governance Safety System (LGSS) - Master Header
 *
 * WHAT: Central include file for the LGSS safety subsystem
 * WHY:  Provide convenient single-header access to all LGSS components
 * HOW:  Includes all LGSS component headers and provides unified initialization
 *
 * ARCHITECTURE OVERVIEW:
 *
 *   +-----------------------------------------------------------------+
 *   |                 LGSS (Layered Governance Safety System)          |
 *   +-----------------------------------------------------------------+
 *   |                                                                   |
 *   |   +-------------------+     +-------------------+                |
 *   |   | Safety Knowledge  | --> | Action            |                |
 *   |   | Base (A1)         |     | Interceptor (A2)  |                |
 *   |   +-------------------+     +-------------------+                |
 *   |            |                         |                           |
 *   |            v                         v                           |
 *   |   +-------------------+     +-------------------+                |
 *   |   | LGSS Loader       |     | Override          |                |
 *   |   | (A1)              |     | Controller (A2)   |                |
 *   |   +-------------------+     +-------------------+                |
 *   |                                      |                           |
 *   |   +----------------------------------+-------------------+       |
 *   |   |                          |                           |       |
 *   |   v                          v                           v       |
 *   |  +----------+  +------------+  +-----------+  +--------+        |
 *   |  | Ethics   |  | Plasticity |  | Executive |  | Bio-   |        |
 *   |  | Bridge   |  | Bridge     |  | Bridge    |  | Async  |        |
 *   |  +----------+  +------------+  +-----------+  +--------+        |
 *   |                                                                   |
 *   |   +-----------------------------------------------------------+  |
 *   |   |               Output Channel Gates (A7)                   |  |
 *   |   | +----------+  +----------+  +------------+                |  |
 *   |   | | Motor    |  | Speech   |  | Autonomic  |                |  |
 *   |   | | Gate     |  | Gate     |  | Gate       |                |  |
 *   |   | +----------+  +----------+  +------------+                |  |
 *   |   +-----------------------------------------------------------+  |
 *   |                                                                   |
 *   |   +-----------------------------------------------------------+  |
 *   |   |            Learning Safety Guards (A8-A9)                 |  |
 *   |   | +----------+  +--------+  +---------+  +---------+        |  |
 *   |   | | STDP     |  | Train  |  | Reward  |  | VTA     |        |  |
 *   |   | | Guard    |  | Guard  |  | Align   |  | Guard   |        |  |
 *   |   | +----------+  +--------+  +---------+  +---------+        |  |
 *   |   +-----------------------------------------------------------+  |
 *   |                                                                   |
 *   |   +-----------------------------------------------------------+  |
 *   |   |          Perception & Cognitive Safety (A10-A11)          |  |
 *   |   | +-----------+  +-----------+  +-----------+               |  |
 *   |   | | Input     |  | Attention |  | Working   |               |  |
 *   |   | | Validator |  | Guard     |  | Memory    |               |  |
 *   |   | +-----------+  +-----------+  +-----------+               |  |
 *   |   +-----------------------------------------------------------+  |
 *   +-----------------------------------------------------------------+
 *
 * DESIGN PRINCIPLES:
 * 1. Defense in Depth: Multiple independent safety layers
 * 2. Fail-Safe Defaults: System fails to OFF, not PERMISSIVE
 * 3. Least Privilege: Actions require explicit permission
 * 4. Immutability: Safety rules locked after initialization
 * 5. Auditability: All decisions logged with full context
 *
 * USAGE:
 *   #include "security/lgss/nimcp_lgss.h"
 *
 *   // Initialize LGSS for a brain
 *   lgss_context_t* lgss = lgss_create(&config);
 *   lgss_load_rules(lgss, "alignment/LGSS_core_rules.json");
 *   lgss_lock(lgss);
 *
 *   // Evaluate an action
 *   lgss_decision_t decision;
 *   lgss_evaluate(lgss, &action_context, &decision);
 *
 *   // Cleanup
 *   lgss_destroy(lgss);
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#ifndef NIMCP_LGSS_H
#define NIMCP_LGSS_H

//=============================================================================
// Core Type Headers (Only these - avoid type conflicts from other LGSS headers)
//=============================================================================

/* A1: Symbolic Logic Safety Extension - Core types */
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_lgss_loader.h"

/*
 * NOTE: The following headers are intentionally NOT included here to avoid
 * type conflicts. They were created with their own type definitions that
 * conflict with types defined in other parts of the codebase.
 *
 * Components can be included individually as needed:
 *   - security/lgss/nimcp_lgss_action_interceptor.h
 *   - security/lgss/nimcp_lgss_override_controller.h
 *   - security/lgss/nimcp_lgss_telemetry.h
 *   - security/lgss/bridges/nimcp_lgss_*.h
 *   - security/lgss/gates/nimcp_lgss_*.h
 *   - security/lgss/learning/nimcp_lgss_*.h
 *   - security/lgss/reward/nimcp_lgss_*.h
 *   - security/lgss/perception/nimcp_lgss_*.h
 *   - security/lgss/cognitive/nimcp_lgss_*.h
 */

//=============================================================================
// Forward Declarations for Opaque Types
//=============================================================================

/* Forward declare opaque types used by the LGSS unified API */
struct action_interceptor_impl;
struct override_controller_impl;
struct lgss_telemetry;

/* Type aliases for opaque pointers */
typedef struct action_interceptor_impl* action_interceptor_t;
typedef struct override_controller_impl* override_controller_t;

//=============================================================================
// LGSS Unified Context
//=============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** @brief LGSS context magic number ('LGSS') */
#define NIMCP_LGSS_MAGIC 0x4C475353

/** @brief LGSS version - major */
#define NIMCP_LGSS_VERSION_MAJOR 1

/** @brief LGSS version - minor */
#define NIMCP_LGSS_VERSION_MINOR 0

/** @brief LGSS version - patch */
#define NIMCP_LGSS_VERSION_PATCH 0

/** @brief Maximum path length for rules file */
#define NIMCP_LGSS_MAX_PATH 512

/*=============================================================================
 * LGSS STATUS ENUMERATION
 *============================================================================*/

/**
 * @brief LGSS subsystem status
 */
typedef enum {
    LGSS_STATUS_UNINITIALIZED = 0,   /**< Not yet initialized */
    LGSS_STATUS_LOADING,              /**< Loading rules */
    LGSS_STATUS_COMPILING,            /**< Compiling rules to FOL */
    LGSS_STATUS_LOCKING,              /**< Locking safety KB */
    LGSS_STATUS_ACTIVE,               /**< Fully active and enforcing */
    LGSS_STATUS_DEGRADED,             /**< Active but with warnings */
    LGSS_STATUS_HALTED,               /**< Emergency halt state */
    LGSS_STATUS_ERROR                 /**< Error state */
} lgss_status_t;

/*=============================================================================
 * LGSS CONFIGURATION
 *============================================================================*/

/**
 * @brief LGSS configuration structure
 *
 * WHAT: Configuration options for LGSS initialization
 * WHY:  Allow customization of safety behavior
 * HOW:  Passed to lgss_create()
 */
typedef struct {
    /** @brief Path to LGSS core rules JSON file */
    char rules_path[NIMCP_LGSS_MAX_PATH];

    /** @brief Maximum number of safety rules */
    uint32_t max_rules;

    /** @brief Default evaluation timeout (milliseconds) */
    uint32_t default_timeout_ms;

    /** @brief Enable fail-safe mode (deny on error) */
    bool fail_safe_enabled;

    /** @brief Enable telemetry logging */
    bool telemetry_enabled;

    /** @brief Enable integrity verification on each evaluation */
    bool verify_integrity_on_eval;

    /** @brief Auto-lock rules after loading */
    bool auto_lock;

    /** @brief Enable bio-async message integration */
    bool bio_async_enabled;

    /** @brief Enable ethics bridge integration */
    bool ethics_bridge_enabled;

    /** @brief Enable plasticity bridge integration */
    bool plasticity_bridge_enabled;

    /** @brief Enable output gates (motor, speech, autonomic) */
    bool output_gates_enabled;

    /** @brief Enable learning guards */
    bool learning_guards_enabled;

    /** @brief Enable perception guards */
    bool perception_guards_enabled;

    /** @brief Enable cognitive guards (attention, WM) */
    bool cognitive_guards_enabled;
} lgss_config_t;

/*=============================================================================
 * LGSS CONTEXT (OPAQUE)
 *============================================================================*/

/**
 * @brief Opaque LGSS context structure
 *
 * WHAT: Central context holding all LGSS components
 * WHY:  Provide unified access to safety subsystem
 * HOW:  Contains safety KB, AIx, bridges, gates, and guards
 */
typedef struct lgss_context lgss_context_t;

/*=============================================================================
 * LGSS STATISTICS
 *============================================================================*/

/**
 * @brief Comprehensive LGSS statistics
 */
typedef struct {
    /** @brief Current LGSS status */
    lgss_status_t status;

    /** @brief Number of rules loaded */
    uint32_t rules_loaded;

    /** @brief Whether safety KB is locked */
    bool kb_locked;

    /** @brief Total evaluations performed */
    uint64_t total_evaluations;

    /** @brief Actions denied */
    uint64_t actions_denied;

    /** @brief Actions escalated */
    uint64_t actions_escalated;

    /** @brief Actions allowed */
    uint64_t actions_allowed;

    /** @brief Integrity checks performed */
    uint64_t integrity_checks;

    /** @brief Integrity failures detected */
    uint64_t integrity_failures;

    /** @brief Override commands received */
    uint64_t override_commands;

    /** @brief Override commands executed */
    uint64_t override_executed;

    /** @brief Average evaluation time (microseconds) */
    float avg_eval_time_us;

    /** @brief Time since LGSS activation (milliseconds) */
    uint64_t uptime_ms;

    /** @brief KB integrity hash (first 8 bytes) */
    uint64_t kb_hash_prefix;
} lgss_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize LGSS configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int lgss_config_init(lgss_config_t* config);

/**
 * @brief Create LGSS context
 *
 * WHAT: Create and initialize the LGSS subsystem
 * WHY:  Entry point for LGSS integration
 * HOW:  Allocates context, creates all components
 *
 * @param config Configuration (NULL for defaults)
 * @return LGSS context, or NULL on failure
 *
 * NOTE: Call lgss_destroy() when done
 */
lgss_context_t* lgss_create(const lgss_config_t* config);

/**
 * @brief Destroy LGSS context
 *
 * @param lgss Context to destroy (NULL safe)
 */
void lgss_destroy(lgss_context_t* lgss);

/**
 * @brief Load safety rules from JSON file
 *
 * @param lgss LGSS context
 * @param rules_path Path to JSON rules file
 * @return Number of rules loaded, or -1 on error
 */
int lgss_load_rules(lgss_context_t* lgss, const char* rules_path);

/**
 * @brief Lock the safety knowledge base (IRREVERSIBLE)
 *
 * CRITICAL: This operation is IRREVERSIBLE for the lifetime of the context.
 *           After locking, rules cannot be added, removed, or modified.
 *
 * @param lgss LGSS context
 * @return 0 on success, -1 on error
 */
int lgss_lock(lgss_context_t* lgss);

/**
 * @brief Check if LGSS is locked
 *
 * @param lgss LGSS context
 * @return true if locked, false otherwise
 */
bool lgss_is_locked(const lgss_context_t* lgss);

/**
 * @brief Get current LGSS status
 *
 * @param lgss LGSS context
 * @return Current status
 */
lgss_status_t lgss_get_status(const lgss_context_t* lgss);

/*=============================================================================
 * EVALUATION FUNCTIONS
 *============================================================================*/

/**
 * @brief Evaluate an action against safety rules
 *
 * WHAT: Core safety evaluation function
 * WHY:  Determine if an action is safe to execute
 * HOW:  Checks action against all loaded safety rules
 *
 * @param lgss LGSS context
 * @param context Action context to evaluate
 * @param result Output evaluation result
 * @return 0 on success, -1 on error
 *
 * NOTE: On error, result->action is set to SAFETY_ACTION_DENY (fail-safe)
 */
int lgss_evaluate(
    lgss_context_t* lgss,
    const safety_action_context_t* context,
    safety_evaluation_t* result
);

/**
 * @brief Quick safety check (returns action only)
 *
 * @param lgss LGSS context
 * @param context Action context
 * @return Safety action (DENY on error)
 */
safety_action_t lgss_check(
    lgss_context_t* lgss,
    const safety_action_context_t* context
);

/*=============================================================================
 * INTEGRITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Verify safety KB integrity
 *
 * @param lgss LGSS context
 * @return 0 if integrity verified, -1 on mismatch or error
 */
int lgss_verify_integrity(lgss_context_t* lgss);

/**
 * @brief Get safety KB hash
 *
 * @param lgss LGSS context
 * @param hash Output buffer (32 bytes)
 * @return 0 on success, -1 on error
 */
int lgss_get_hash(const lgss_context_t* lgss, uint8_t hash[32]);

/*=============================================================================
 * STATISTICS AND MONITORING
 *============================================================================*/

/**
 * @brief Get LGSS statistics
 *
 * @param lgss LGSS context
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int lgss_get_stats(const lgss_context_t* lgss, lgss_stats_t* stats);

/**
 * @brief Reset LGSS statistics
 *
 * @param lgss LGSS context
 * @return 0 on success, -1 on error
 */
int lgss_reset_stats(lgss_context_t* lgss);

/*=============================================================================
 * COMPONENT ACCESS (FOR INTEGRATION)
 *============================================================================*/

/**
 * @brief Get safety knowledge base
 *
 * @param lgss LGSS context
 * @return Safety KB (read-only if locked)
 */
safety_kb_t* lgss_get_safety_kb(lgss_context_t* lgss);

/**
 * @brief Get action interceptor
 *
 * @param lgss LGSS context
 * @return Action interceptor
 */
action_interceptor_t lgss_get_interceptor(lgss_context_t* lgss);

/**
 * @brief Get override controller
 *
 * @param lgss LGSS context
 * @return Override controller
 */
override_controller_t lgss_get_override_controller(lgss_context_t* lgss);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get LGSS version string
 *
 * @return Version string (e.g., "1.0.0")
 */
const char* lgss_version_string(void);

/**
 * @brief Get human-readable status name
 *
 * @param status LGSS status
 * @return Status name string
 */
const char* lgss_status_name(lgss_status_t status);

/**
 * @brief Log LGSS status to system log
 *
 * @param lgss LGSS context
 */
void lgss_log_status(const lgss_context_t* lgss);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_H */
