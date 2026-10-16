/**
 * @file nimcp_code_generation.h
 * @brief Code Generation Engine for Autonomous Self-Repair
 *
 * WHAT: Generate code fixes for detected errors based on diagnostic analysis
 * WHY:  Enable autonomous self-healing through intelligent code generation
 * HOW:  Template-based fix generation with pattern matching against historical fixes
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                       CODE GENERATION ENGINE                                 │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌────────────────────┐     ┌────────────────────┐     ┌─────────────────┐ │
 * │  │ DIAGNOSTIC INPUT   │     │ FIX TEMPLATES      │     │ CODE IMMUNE     │ │
 * │  │ - error_type       │────>│ - Null checks      │<───>│ - Historical    │ │
 * │  │ - code_location    │     │ - Bounds checks    │     │   patterns      │ │
 * │  │ - code_analysis    │     │ - NaN guards       │     │ - Success rates │ │
 * │  └────────────────────┘     │ - Mutex fixes      │     └─────────────────┘ │
 * │                             │ - Memory cleanup   │                          │
 * │                             └─────────┬──────────┘                          │
 * │                                       │                                     │
 * │                                       ▼                                     │
 * │                    ┌──────────────────────────────────┐                    │
 * │                    │      FIX CANDIDATE RANKING       │                    │
 * │                    │ - Confidence scoring             │                    │
 * │                    │ - Risk assessment                │                    │
 * │                    │ - Historical success matching    │                    │
 * │                    └─────────────────┬────────────────┘                    │
 * │                                      │                                      │
 * │                                      ▼                                      │
 * │                    ┌──────────────────────────────────┐                    │
 * │                    │       GENERATED FIX OUTPUT       │                    │
 * │                    │ - Original code                  │                    │
 * │                    │ - Fixed code                     │                    │
 * │                    │ - Confidence score               │                    │
 * │                    │ - Explanation                    │                    │
 * │                    └──────────────────────────────────┘                    │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * INTEGRATION:
 * - Receives diagnostic_result_t from fault tolerance module
 * - Uses code_analysis_result_t from parietal bridge
 * - Learns from code_immune memory for pattern matching
 * - Outputs to recompiler for validation
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef NIMCP_CODE_GENERATION_H
#define NIMCP_CODE_GENERATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "cognitive/fault_tolerance/nimcp_recovery_parietal_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define CODE_GEN_VERSION            "1.0.0"
#define CODE_GEN_MAX_FIX_CODE       4096    /**< Max generated code size */
#define CODE_GEN_MAX_EXPLANATION    512     /**< Max explanation length */
#define CODE_GEN_MAX_CANDIDATES     16      /**< Max fix candidates per error */
#define CODE_GEN_MAX_TEMPLATES      64      /**< Max loaded fix templates */
#define CODE_GEN_MAGIC              0x434F4447  /**< 'CODG' */

//=============================================================================
// Fix Strategy Types
//=============================================================================

/**
 * @brief Fix strategy enumeration
 *
 * WHAT: Categories of code fixes the engine can generate
 * WHY:  Map error types to appropriate fix strategies
 * HOW:  Template-based generation for each strategy type
 */
typedef enum {
    FIX_STRATEGY_NONE = 0,              /**< No fix strategy */
    FIX_STRATEGY_NULL_CHECK,            /**< Add null pointer guard */
    FIX_STRATEGY_BOUNDS_CHECK,          /**< Add array bounds check */
    FIX_STRATEGY_DIVISION_GUARD,        /**< Add division by zero check */
    FIX_STRATEGY_INITIALIZATION,        /**< Initialize uninitialized variables */
    FIX_STRATEGY_NAN_GUARD,             /**< Add NaN/Inf checks */
    FIX_STRATEGY_MUTEX_FIX,             /**< Fix deadlock/race conditions */
    FIX_STRATEGY_MEMORY_CLEANUP,        /**< Fix memory leaks */
    FIX_STRATEGY_ERROR_HANDLING,        /**< Add error handling */
    FIX_STRATEGY_TYPE_CAST,             /**< Add proper type casting */
    FIX_STRATEGY_OVERFLOW_GUARD,        /**< Add overflow protection */
    FIX_STRATEGY_ALIGNMENT_FIX,         /**< Fix memory alignment issues */
    FIX_STRATEGY_ASSERTION_FIX,         /**< Fix assertion failures */
    FIX_STRATEGY_CUSTOM                 /**< Custom/learned fix strategy */
} code_fix_strategy_t;

/**
 * @brief Fix complexity level
 */
typedef enum {
    FIX_COMPLEXITY_TRIVIAL = 0,         /**< Single-line fix */
    FIX_COMPLEXITY_SIMPLE,              /**< Few lines, local scope */
    FIX_COMPLEXITY_MODERATE,            /**< Multiple statements */
    FIX_COMPLEXITY_COMPLEX,             /**< Control flow changes */
    FIX_COMPLEXITY_ARCHITECTURAL        /**< Structural changes needed */
} fix_complexity_t;

/**
 * @brief Fix verification status
 */
typedef enum {
    FIX_STATUS_PROPOSED = 0,            /**< Just generated */
    FIX_STATUS_COMPILED,                /**< Compiled successfully */
    FIX_STATUS_TESTED,                  /**< Passed sandbox tests */
    FIX_STATUS_VALIDATED,               /**< Passed full validation */
    FIX_STATUS_APPLIED,                 /**< Applied to running system */
    FIX_STATUS_COMMITTED,               /**< Committed to source */
    FIX_STATUS_FAILED                   /**< Fix failed validation */
} fix_status_t;

//=============================================================================
// Generated Fix Structure
//=============================================================================

/**
 * @brief Generated code fix
 *
 * WHAT: Complete fix specification with original and fixed code
 * WHY:  Encapsulate all information needed to apply and track a fix
 * HOW:  Store code, location, confidence, and metadata
 */
typedef struct {
    uint64_t fix_id;                            /**< Unique fix identifier */
    code_fix_strategy_t strategy;               /**< Fix strategy used */
    fix_complexity_t complexity;                /**< Estimated complexity */
    fix_status_t status;                        /**< Current status */

    /* Code content */
    char original_code[CODE_GEN_MAX_FIX_CODE];  /**< Original problematic code */
    char fixed_code[CODE_GEN_MAX_FIX_CODE];     /**< Generated fixed code */

    /* Location */
    char source_file[512];                      /**< Source file path */
    char function_name[128];                    /**< Function name */
    uint32_t start_line;                        /**< Start line of fix */
    uint32_t end_line;                          /**< End line of fix */

    /* Confidence and risk */
    float confidence;                           /**< Fix confidence [0,1] */
    float risk_score;                           /**< Risk of side effects [0,1] */
    float historical_success_rate;              /**< Past success rate for similar */

    /* Explanation */
    char explanation[CODE_GEN_MAX_EXPLANATION]; /**< Human-readable explanation */
    char root_cause[256];                       /**< Identified root cause */

    /* Traceability */
    uint64_t diagnostic_id;                     /**< Source diagnostic ID */
    uint64_t timestamp;                         /**< Generation timestamp */
    uint64_t pattern_id;                        /**< Matched historical pattern (0 if none) */
} generated_fix_t;

/**
 * @brief Fix candidate set
 *
 * WHAT: Collection of candidate fixes for a single error
 * WHY:  Rank multiple approaches, select best
 * HOW:  Array of generated_fix_t with ranking metadata
 */
typedef struct {
    generated_fix_t candidates[CODE_GEN_MAX_CANDIDATES]; /**< Candidate fixes */
    uint32_t count;                             /**< Number of candidates */
    uint32_t selected_index;                    /**< Index of selected fix */
    float total_confidence;                     /**< Combined confidence */
} fix_candidate_set_t;

//=============================================================================
// Code Generation Input/Output
//=============================================================================

/**
 * @brief Code generation request
 *
 * WHAT: Input specification for code generation
 * WHY:  Provide all context needed to generate appropriate fix
 * HOW:  Combine diagnostic result with code analysis
 */
typedef struct {
    /* Error information */
    diagnostic_result_t* diagnosis;             /**< Diagnostic result */
    code_analysis_result_t* code_analysis;      /**< Parietal code analysis */
    code_location_t location;                   /**< Failure location */

    /* Source code context */
    const char* source_code;                    /**< Source code around error */
    uint32_t context_lines_before;              /**< Lines of context before */
    uint32_t context_lines_after;               /**< Lines of context after */

    /* Generation options */
    code_fix_strategy_t preferred_strategy;     /**< Preferred strategy (NONE for auto) */
    float min_confidence;                       /**< Minimum confidence threshold */
    float max_risk;                             /**< Maximum acceptable risk */
    uint32_t max_candidates;                    /**< Max candidates to generate */
    bool allow_architectural_changes;           /**< Allow complex fixes */
    bool use_historical_patterns;               /**< Match against code_immune */
} code_gen_request_t;

/**
 * @brief Code generation result
 *
 * WHAT: Output from code generation attempt
 * WHY:  Report success/failure with generated fixes
 * HOW:  Candidate set plus generation metadata
 */
typedef struct {
    bool success;                               /**< Generation succeeded */
    fix_candidate_set_t candidates;             /**< Generated candidates */
    generated_fix_t* best_fix;                  /**< Pointer to best candidate */
    char error_message[256];                    /**< Error if failed */
    uint64_t generation_time_us;                /**< Generation time */
    uint32_t patterns_matched;                  /**< Historical patterns matched */
} code_gen_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Code generation engine configuration
 */
typedef struct {
    /* Generation behavior */
    float default_min_confidence;               /**< Default min confidence (0.7) */
    float default_max_risk;                     /**< Default max risk (0.3) */
    uint32_t default_max_candidates;            /**< Default max candidates (5) */

    /* Template paths */
    const char* template_directory;             /**< Directory for fix templates */

    /* Learning integration */
    bool enable_code_immune_learning;           /**< Learn from code_immune */
    bool enable_pattern_matching;               /**< Match historical patterns */
    float pattern_match_threshold;              /**< Min similarity for match */

    /* Safety */
    bool require_human_approval_complex;        /**< Require approval for complex */
    bool auto_rollback_on_regression;           /**< Auto-rollback if regression */

    /* Logging */
    bool verbose_logging;                       /**< Enable verbose output */
} code_gen_config_t;

/**
 * @brief Code generation statistics
 */
typedef struct {
    uint64_t fixes_generated;                   /**< Total fixes generated */
    uint64_t fixes_compiled;                    /**< Fixes that compiled */
    uint64_t fixes_validated;                   /**< Fixes that passed validation */
    uint64_t fixes_applied;                     /**< Fixes applied to runtime */
    uint64_t fixes_committed;                   /**< Fixes committed to source */
    uint64_t fixes_failed;                      /**< Fixes that failed */
    uint64_t patterns_matched;                  /**< Historical patterns matched */
    float avg_confidence;                       /**< Average fix confidence */
    float avg_generation_time_us;               /**< Average generation time */
    uint64_t by_strategy[16];                   /**< Count by strategy */
} code_gen_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct code_gen_engine code_gen_engine_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
code_gen_config_t code_gen_default_config(void);

/**
 * @brief Create code generation engine
 *
 * WHAT: Initialize code generation engine
 * WHY:  Entry point for code generation capability
 * HOW:  Load templates, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return Engine handle or NULL on failure
 */
code_gen_engine_t* code_gen_create(const code_gen_config_t* config);

/**
 * @brief Destroy code generation engine
 *
 * @param engine Engine handle (NULL safe)
 */
void code_gen_destroy(code_gen_engine_t* engine);

/**
 * @brief Check if engine is ready
 *
 * @param engine Engine handle
 * @return true if ready to generate fixes
 */
bool code_gen_is_ready(const code_gen_engine_t* engine);

//=============================================================================
// Core Generation Functions
//=============================================================================

/**
 * @brief Generate fix candidates for error
 *
 * WHAT: Generate candidate fixes for diagnosed error
 * WHY:  Core code generation functionality
 * HOW:  Analyze error, select strategy, instantiate templates
 *
 * @param engine Engine handle
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int code_gen_generate_candidates(
    code_gen_engine_t* engine,
    const code_gen_request_t* request,
    code_gen_result_t* result
);

/**
 * @brief Select best fix from candidates
 *
 * WHAT: Rank and select optimal fix from candidate set
 * WHY:  Choose most appropriate fix for deployment
 * HOW:  Score by confidence, risk, historical success
 *
 * @param engine Engine handle
 * @param candidates Candidate set
 * @param selected Output selected fix
 * @return 0 on success, -1 if no valid candidate
 */
int code_gen_select_best_fix(
    code_gen_engine_t* engine,
    const fix_candidate_set_t* candidates,
    generated_fix_t* selected
);

/**
 * @brief Generate fix using specific strategy
 *
 * WHAT: Generate fix using specified strategy
 * WHY:  Allow manual strategy selection
 * HOW:  Directly instantiate template for strategy
 *
 * @param engine Engine handle
 * @param strategy Strategy to use
 * @param location Code location
 * @param source_code Source code context
 * @param fix Output generated fix
 * @return 0 on success, -1 on error
 */
int code_gen_generate_with_strategy(
    code_gen_engine_t* engine,
    code_fix_strategy_t strategy,
    const code_location_t* location,
    const char* source_code,
    generated_fix_t* fix
);

//=============================================================================
// Strategy Selection Functions
//=============================================================================

/**
 * @brief Select appropriate strategy for error type
 *
 * WHAT: Map error type to fix strategy
 * WHY:  Automated strategy selection
 * HOW:  Error type → strategy mapping with confidence
 *
 * @param engine Engine handle
 * @param error_type Error type from diagnostics
 * @param code_analysis Optional code analysis for context
 * @param strategy Output selected strategy
 * @param confidence Output confidence in strategy selection
 * @return 0 on success
 */
int code_gen_select_strategy(
    code_gen_engine_t* engine,
    error_type_t error_type,
    const code_analysis_result_t* code_analysis,
    code_fix_strategy_t* strategy,
    float* confidence
);

/**
 * @brief Get compatible strategies for error type
 *
 * WHAT: List all strategies that could fix error type
 * WHY:  Generate multiple candidates
 * HOW:  Error type → strategy set mapping
 *
 * @param engine Engine handle
 * @param error_type Error type
 * @param strategies Output strategy array
 * @param max_strategies Max strategies to return
 * @return Number of compatible strategies
 */
int code_gen_get_compatible_strategies(
    code_gen_engine_t* engine,
    error_type_t error_type,
    code_fix_strategy_t* strategies,
    uint32_t max_strategies
);

//=============================================================================
// Pattern Matching Functions
//=============================================================================

/**
 * @brief Match against historical fix patterns
 *
 * WHAT: Find similar errors in code_immune memory
 * WHY:  Reuse proven fixes
 * HOW:  Pattern matching on error signature + code structure
 *
 * @param engine Engine handle
 * @param request Generation request
 * @param pattern_id Output matched pattern ID (0 if none)
 * @param similarity Output similarity score
 * @return 0 if match found, -1 if no match
 */
int code_gen_match_historical_pattern(
    code_gen_engine_t* engine,
    const code_gen_request_t* request,
    uint64_t* pattern_id,
    float* similarity
);

/**
 * @brief Learn from fix outcome
 *
 * WHAT: Record fix success/failure for future matching
 * WHY:  Improve future fix generation
 * HOW:  Update pattern database with outcome
 *
 * @param engine Engine handle
 * @param fix Fix that was applied
 * @param success Whether fix succeeded
 * @return 0 on success
 */
int code_gen_learn_from_outcome(
    code_gen_engine_t* engine,
    const generated_fix_t* fix,
    bool success
);

//=============================================================================
// Confidence Scoring Functions
//=============================================================================

/**
 * @brief Calculate fix confidence score
 *
 * WHAT: Score confidence in generated fix
 * WHY:  Filter low-confidence fixes
 * HOW:  Multi-factor scoring: strategy match, complexity, patterns
 *
 * @param engine Engine handle
 * @param fix Fix to score
 * @param request Original request for context
 * @return Confidence score [0,1]
 */
float code_gen_calculate_confidence(
    code_gen_engine_t* engine,
    const generated_fix_t* fix,
    const code_gen_request_t* request
);

/**
 * @brief Calculate fix risk score
 *
 * WHAT: Score risk of side effects
 * WHY:  Filter high-risk fixes
 * HOW:  Analyze scope, complexity, dependencies
 *
 * @param engine Engine handle
 * @param fix Fix to score
 * @param code_analysis Code analysis for context
 * @return Risk score [0,1]
 */
float code_gen_calculate_risk(
    code_gen_engine_t* engine,
    const generated_fix_t* fix,
    const code_analysis_result_t* code_analysis
);

//=============================================================================
// Template Management Functions
//=============================================================================

/**
 * @brief Load fix templates from directory
 *
 * WHAT: Load template definitions from files
 * WHY:  Externalize fix patterns
 * HOW:  Parse template files, register with engine
 *
 * @param engine Engine handle
 * @param directory Template directory path
 * @return Number of templates loaded, -1 on error
 */
int code_gen_load_templates(
    code_gen_engine_t* engine,
    const char* directory
);

/**
 * @brief Register custom fix template
 *
 * WHAT: Add custom template programmatically
 * WHY:  Allow runtime template addition
 * HOW:  Register template with engine
 *
 * @param engine Engine handle
 * @param strategy Strategy template applies to
 * @param template_code Template code with placeholders
 * @param description Template description
 * @return 0 on success
 */
int code_gen_register_template(
    code_gen_engine_t* engine,
    code_fix_strategy_t strategy,
    const char* template_code,
    const char* description
);

//=============================================================================
// Fix Status Management
//=============================================================================

/**
 * @brief Update fix status
 *
 * WHAT: Track fix through lifecycle
 * WHY:  Enable status-based queries and auditing
 * HOW:  Update status field, record timestamp
 *
 * @param engine Engine handle
 * @param fix_id Fix ID
 * @param new_status New status
 * @return 0 on success
 */
int code_gen_update_status(
    code_gen_engine_t* engine,
    uint64_t fix_id,
    fix_status_t new_status
);

/**
 * @brief Get fix by ID
 *
 * @param engine Engine handle
 * @param fix_id Fix ID
 * @return Fix or NULL if not found
 */
const generated_fix_t* code_gen_get_fix(
    code_gen_engine_t* engine,
    uint64_t fix_id
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get generation statistics
 *
 * @param engine Engine handle
 * @param stats Output statistics
 * @return 0 on success
 */
int code_gen_get_stats(
    const code_gen_engine_t* engine,
    code_gen_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param engine Engine handle
 */
void code_gen_reset_stats(code_gen_engine_t* engine);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get strategy name string
 *
 * @param strategy Strategy enum
 * @return Strategy name (static)
 */
const char* code_gen_strategy_name(code_fix_strategy_t strategy);

/**
 * @brief Get complexity name string
 *
 * @param complexity Complexity enum
 * @return Complexity name (static)
 */
const char* code_gen_complexity_name(fix_complexity_t complexity);

/**
 * @brief Get status name string
 *
 * @param status Status enum
 * @return Status name (static)
 */
const char* code_gen_status_name(fix_status_t status);

/**
 * @brief Get engine version string
 *
 * @return Version string
 */
const char* code_gen_version(void);

//=============================================================================
// Bio-Async Communication Functions
//=============================================================================

/**
 * @brief Broadcast code generation result via bio-async
 *
 * WHAT: Notify other modules about code generation completion
 * WHY:  Enable cross-module coordination during repair
 * HOW:  Send message via bio-router to self-repair coordinator
 *
 * @param engine Engine handle
 * @param fix_id Fix ID
 * @param success True if generation succeeded
 * @param confidence Fix confidence score
 * @return 0 on success, -1 on error
 */
int code_gen_broadcast_result(
    code_gen_engine_t* engine,
    uint64_t fix_id,
    bool success,
    float confidence
);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process incoming bio-async messages
 * WHY:  Handle requests from other modules
 * HOW:  Call bio_router_process_inbox
 *
 * @param engine Engine handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t code_gen_process_messages(
    code_gen_engine_t* engine,
    uint32_t max_messages
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CODE_GENERATION_H */
