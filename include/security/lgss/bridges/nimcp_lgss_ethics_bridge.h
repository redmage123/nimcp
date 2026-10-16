/**
 * @file nimcp_lgss_ethics_bridge.h
 * @brief LGSS-Ethics Integration Bridge
 *
 * WHAT: Bridge connecting LGSS safety system to the ethics engine
 * WHY:  LGSS provides L0 (highest priority) safety layer above ethics directives
 * HOW:  Intercepts ethics evaluations, applies LGSS first, then ethics
 *
 * PRIORITY HIERARCHY:
 *   L0: LGSS Safety KB (THIS LAYER - HIGHEST PRIORITY)
 *       - LGSS DENY -> immediate block (no further evaluation)
 *       - LGSS ESCALATE -> human approval required
 *       - LGSS ALLOW -> continue to L1
 *
 *   L1: First Law (harm prevention)
 *   L2: Combinatorial Harm
 *   L3: Golden Rule
 *   L4: Second Law (commands)
 *   L5: Third Law (preservation)
 *
 * INTEGRATION PATTERN:
 *   Before (without LGSS):
 *     ethics_evaluate() -> core_directives_evaluate()
 *
 *   After (with LGSS):
 *     lgss_ethics_evaluate() -> lgss_check() -> ethics_evaluate()
 *
 * SECURITY NOTE:
 *   The LGSS layer CANNOT be bypassed. All ethics evaluations MUST
 *   route through this bridge when LGSS is active.
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#ifndef NIMCP_LGSS_ETHICS_BRIDGE_H
#define NIMCP_LGSS_ETHICS_BRIDGE_H

#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

/* Forward declare types to avoid circular dependencies */
struct lgss_context;
struct action_interceptor;
struct ethics_engine;
struct core_directives;

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** @brief Ethics bridge magic number ('LETH') */
#define NIMCP_LGSS_ETHICS_BRIDGE_MAGIC 0x4C455448

/** @brief Maximum number of ethics layers */
#define LGSS_ETHICS_MAX_LAYERS 16

/** @brief LGSS layer index (L0 - highest priority) */
#define LGSS_ETHICS_LAYER_LGSS 0

/** @brief First Law layer index (L1) */
#define LGSS_ETHICS_LAYER_FIRST_LAW 1

/*=============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Ethics evaluation result with LGSS integration
 */
typedef enum {
    /** @brief Action is ethically permissible */
    LGSS_ETHICS_ALLOW = 0,

    /** @brief Action is blocked by LGSS (L0) */
    LGSS_ETHICS_BLOCKED_BY_LGSS = 1,

    /** @brief Action is blocked by ethics (L1-L5) */
    LGSS_ETHICS_BLOCKED_BY_ETHICS = 2,

    /** @brief Action requires LGSS escalation */
    LGSS_ETHICS_ESCALATE_LGSS = 3,

    /** @brief Action requires ethics escalation */
    LGSS_ETHICS_ESCALATE_ETHICS = 4,

    /** @brief Evaluation error */
    LGSS_ETHICS_ERROR = 5
} lgss_ethics_result_t;

/**
 * @brief Ethics layer that triggered decision
 */
typedef enum {
    LGSS_TRIGGERED_BY_NONE = -1,
    LGSS_TRIGGERED_BY_LGSS = 0,        /* L0: LGSS Safety */
    LGSS_TRIGGERED_BY_FIRST_LAW = 1,   /* L1: First Law */
    LGSS_TRIGGERED_BY_COMBINATORIAL = 2, /* L2: Combinatorial Harm */
    LGSS_TRIGGERED_BY_GOLDEN_RULE = 3, /* L3: Golden Rule */
    LGSS_TRIGGERED_BY_SECOND_LAW = 4,  /* L4: Second Law */
    LGSS_TRIGGERED_BY_THIRD_LAW = 5    /* L5: Third Law */
} lgss_ethics_trigger_layer_t;

/*=============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Ethics bridge configuration
 */
typedef struct {
    /** @brief Enable LGSS as L0 layer */
    bool lgss_enabled;

    /** @brief Continue to ethics layers even if LGSS allows */
    bool always_check_ethics;

    /** @brief Log all evaluations */
    bool log_evaluations;

    /** @brief Fail-safe: deny on error */
    bool fail_safe;

    /** @brief Timeout for LGSS evaluation (milliseconds) */
    uint32_t lgss_timeout_ms;

    /** @brief Timeout for ethics evaluation (milliseconds) */
    uint32_t ethics_timeout_ms;
} lgss_ethics_bridge_config_t;

/**
 * @brief Combined ethics evaluation result
 */
typedef struct {
    /** @brief Final result */
    lgss_ethics_result_t result;

    /** @brief Layer that triggered the decision */
    lgss_ethics_trigger_layer_t triggered_by;

    /** @brief LGSS evaluation result */
    safety_evaluation_t lgss_eval;

    /** @brief Whether LGSS was consulted */
    bool lgss_consulted;

    /** @brief Whether ethics was consulted */
    bool ethics_consulted;

    /** @brief Human-readable explanation */
    char explanation[512];

    /** @brief Confidence in decision [0,1] */
    float confidence;

    /** @brief Total evaluation time (microseconds) */
    uint64_t eval_time_us;
} lgss_ethics_evaluation_t;

/**
 * @brief Opaque ethics bridge context
 */
typedef struct lgss_ethics_bridge lgss_ethics_bridge_t;

/**
 * @brief Ethics bridge statistics
 */
typedef struct {
    /** @brief Total evaluations */
    uint64_t total_evaluations;

    /** @brief Blocked by LGSS */
    uint64_t blocked_by_lgss;

    /** @brief Blocked by ethics */
    uint64_t blocked_by_ethics;

    /** @brief Escalated by LGSS */
    uint64_t escalated_by_lgss;

    /** @brief Escalated by ethics */
    uint64_t escalated_by_ethics;

    /** @brief Allowed */
    uint64_t allowed;

    /** @brief Errors */
    uint64_t errors;

    /** @brief Average LGSS eval time (us) */
    float avg_lgss_time_us;

    /** @brief Average ethics eval time (us) */
    float avg_ethics_time_us;

    /** @brief Triggers by layer */
    uint64_t triggers_by_layer[LGSS_ETHICS_MAX_LAYERS];
} lgss_ethics_bridge_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize ethics bridge configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_config_init(lgss_ethics_bridge_config_t* config);

/**
 * @brief Create ethics bridge
 *
 * @param lgss LGSS context (must be active)
 * @param ethics Ethics engine (optional, can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Ethics bridge, or NULL on failure
 */
lgss_ethics_bridge_t* lgss_ethics_bridge_create(
    struct lgss_context* lgss,
    struct ethics_engine* ethics,
    const lgss_ethics_bridge_config_t* config
);

/**
 * @brief Destroy ethics bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void lgss_ethics_bridge_destroy(lgss_ethics_bridge_t* bridge);

/**
 * @brief Set the ethics engine (can be done after creation)
 *
 * @param bridge Ethics bridge
 * @param ethics Ethics engine
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_set_ethics(
    lgss_ethics_bridge_t* bridge,
    struct ethics_engine* ethics
);

/**
 * @brief Set the core directives (can be done after creation)
 *
 * @param bridge Ethics bridge
 * @param directives Core directives
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_set_directives(
    lgss_ethics_bridge_t* bridge,
    struct core_directives* directives
);

/*=============================================================================
 * EVALUATION FUNCTIONS
 *============================================================================*/

/**
 * @brief Evaluate action through LGSS + Ethics layers
 *
 * WHAT: Primary evaluation function combining LGSS and ethics
 * WHY:  Ensure all actions pass through complete safety stack
 * HOW:  LGSS first (L0), then ethics (L1-L5) if LGSS allows
 *
 * EVALUATION ORDER:
 * 1. Check LGSS (L0)
 *    - DENY -> return BLOCKED_BY_LGSS
 *    - ESCALATE -> return ESCALATE_LGSS
 *    - ALLOW -> continue to step 2
 * 2. Check Ethics (L1-L5) if enabled
 *    - DENY -> return BLOCKED_BY_ETHICS
 *    - ESCALATE -> return ESCALATE_ETHICS
 *    - ALLOW -> return ALLOW
 *
 * @param bridge Ethics bridge
 * @param context Action context to evaluate
 * @param result Output evaluation result
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_evaluate(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context,
    lgss_ethics_evaluation_t* result
);

/**
 * @brief Quick check (returns result only)
 *
 * @param bridge Ethics bridge
 * @param context Action context
 * @return Ethics result (ERROR on failure)
 */
lgss_ethics_result_t lgss_ethics_bridge_check(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context
);

/**
 * @brief Check only LGSS layer (skip ethics)
 *
 * @param bridge Ethics bridge
 * @param context Action context
 * @param result Output evaluation result
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_check_lgss_only(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context,
    lgss_ethics_evaluation_t* result
);

/**
 * @brief Check only ethics layers (skip LGSS)
 *
 * WARNING: This bypasses LGSS safety. Use only for testing.
 *
 * @param bridge Ethics bridge
 * @param context Action context
 * @param result Output evaluation result
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_check_ethics_only(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context,
    lgss_ethics_evaluation_t* result
);

/*=============================================================================
 * LAYER MANAGEMENT
 *============================================================================*/

/**
 * @brief Enable/disable LGSS layer
 *
 * WARNING: Disabling LGSS removes L0 safety. Use with extreme caution.
 *
 * @param bridge Ethics bridge
 * @param enabled Whether to enable LGSS
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_set_lgss_enabled(
    lgss_ethics_bridge_t* bridge,
    bool enabled
);

/**
 * @brief Check if LGSS layer is enabled
 *
 * @param bridge Ethics bridge
 * @return true if enabled
 */
bool lgss_ethics_bridge_is_lgss_enabled(const lgss_ethics_bridge_t* bridge);

/**
 * @brief Enable/disable ethics layers
 *
 * @param bridge Ethics bridge
 * @param enabled Whether to enable ethics
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_set_ethics_enabled(
    lgss_ethics_bridge_t* bridge,
    bool enabled
);

/**
 * @brief Check if ethics layers are enabled
 *
 * @param bridge Ethics bridge
 * @return true if enabled
 */
bool lgss_ethics_bridge_is_ethics_enabled(const lgss_ethics_bridge_t* bridge);

/*=============================================================================
 * STATISTICS AND MONITORING
 *============================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Ethics bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_get_stats(
    const lgss_ethics_bridge_t* bridge,
    lgss_ethics_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Ethics bridge
 * @return 0 on success, -1 on error
 */
int lgss_ethics_bridge_reset_stats(lgss_ethics_bridge_t* bridge);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get human-readable result name
 *
 * @param result Ethics result
 * @return Result name string
 */
const char* lgss_ethics_result_name(lgss_ethics_result_t result);

/**
 * @brief Get human-readable layer name
 *
 * @param layer Trigger layer
 * @return Layer name string
 */
const char* lgss_ethics_layer_name(lgss_ethics_trigger_layer_t layer);

/**
 * @brief Format evaluation as human-readable string
 *
 * @param eval Evaluation to format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Characters written, or -1 on error
 */
int lgss_ethics_format_evaluation(
    const lgss_ethics_evaluation_t* eval,
    char* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_ETHICS_BRIDGE_H */
