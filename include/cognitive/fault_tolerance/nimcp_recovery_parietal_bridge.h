/**
 * @file nimcp_recovery_parietal_bridge.h
 * @brief Parietal Lobe Integration for Fault Tolerance Recovery
 *
 * WHAT: Bridge connecting parietal lobe capabilities to recovery executive
 * WHY:  Enhanced recovery through spatial code analysis and pattern detection
 * HOW:  Parietal provides code structure analysis, pattern matching, complexity estimation
 *
 * INTEGRATION BENEFITS:
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │                    PARIETAL → RECOVERY INTEGRATION                      │
 * │                                                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐  │
 * │  │  SOFTWARE ENGINEERING ANALYSIS                                   │  │
 * │  │  - Analyze code structure at failure location                    │  │
 * │  │  - Calculate complexity metrics of affected modules              │  │
 * │  │  - Detect code smells suggesting root causes                     │  │
 * │  │  - Dependency graph analysis for impact assessment               │  │
 * │  └─────────────────────────────────────────────────────────────────┘  │
 * │                              ↓                                          │
 * │  ┌─────────────────────────────────────────────────────────────────┐  │
 * │  │  PATTERN DETECTION                                               │  │
 * │  │  - Match failure patterns against historical failures            │  │
 * │  │  - Identify design pattern violations                            │  │
 * │  │  - Symmetry detection for duplicated problematic code            │  │
 * │  └─────────────────────────────────────────────────────────────────┘  │
 * │                              ↓                                          │
 * │  ┌─────────────────────────────────────────────────────────────────┐  │
 * │  │  SPATIAL REASONING                                               │  │
 * │  │  - Visualize code dependency topology                            │  │
 * │  │  - Mental rotation for architecture understanding                │  │
 * │  │  - Spatial queries for affected component discovery              │  │
 * │  └─────────────────────────────────────────────────────────────────┘  │
 * │                              ↓                                          │
 * │  ┌─────────────────────────────────────────────────────────────────┐  │
 * │  │  MATHEMATICAL INTUITION                                          │  │
 * │  │  - Estimate recovery feasibility                                 │  │
 * │  │  - Hypothesis testing for recovery strategies                    │  │
 * │  │  - Quantify expected improvement                                 │  │
 * │  └─────────────────────────────────────────────────────────────────┘  │
 * └────────────────────────────────────────────────────────────────────────┘
 *
 * BIOLOGICAL BASIS:
 * The parietal lobe's spatial reasoning and mathematical capabilities
 * enhance the prefrontal executive's planning and decision-making.
 * This mirrors how posterior parietal cortex supports prefrontal cortex
 * in complex problem-solving and strategic planning.
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_RECOVERY_PARIETAL_BRIDGE_H
#define NIMCP_RECOVERY_PARIETAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/fault_tolerance/nimcp_recovery_executive.h"
#include "cognitive/parietal/nimcp_software_engineering.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/* Parietal lobe - defined in nimcp_parietal.h */
#ifndef NIMCP_PARIETAL_LOBE_T_DEFINED
#define NIMCP_PARIETAL_LOBE_T_DEFINED
typedef struct parietal_lobe parietal_lobe_t;
#endif

//=============================================================================
// Constants
//=============================================================================

#define RECOVERY_PARIETAL_VERSION       "1.0.0"
#define MAX_CODE_ANALYSIS_MODULES       64      /**< Max modules to analyze */
#define MAX_FAILURE_PATTERNS            32      /**< Max historical patterns */
#define MAX_AFFECTED_COMPONENTS         128     /**< Max affected components */

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Code location for failure analysis
 */
typedef struct {
    char file_path[256];            /**< Source file path */
    char function_name[128];        /**< Function name */
    uint32_t line_number;           /**< Line number */
    char module_name[128];          /**< Module/class name */
} code_location_t;

/**
 * @brief Code analysis request for parietal processing
 */
typedef struct {
    code_location_t failure_location;       /**< Where failure occurred */
    diagnostic_result_t* diagnosis;         /**< Diagnostic information */
    bool analyze_dependencies;              /**< Analyze dependency graph */
    bool detect_code_smells;                /**< Detect code smells */
    bool analyze_complexity;                /**< Analyze complexity */
    bool find_similar_patterns;             /**< Match against history */
    uint32_t dependency_depth;              /**< How deep to trace dependencies */
} code_analysis_request_t;

/**
 * @brief Code analysis result from parietal
 */
typedef struct {
    /* Complexity analysis */
    complexity_result_t complexity;         /**< Complexity metrics */
    software_metrics_t metrics;             /**< Software metrics */

    /* Code smells */
    smell_result_t smells[8];               /**< Detected code smells */
    uint32_t smell_count;                   /**< Number of smells */

    /* Dependency analysis */
    dep_graph_t* dependency_graph;          /**< Dependency graph (if requested) */
    bool has_circular_deps;                 /**< Circular dependencies detected */
    uint32_t affected_modules;              /**< Number of affected modules */

    /* Pattern matching */
    struct {
        uint64_t pattern_id;                /**< Historical pattern ID */
        float similarity;                   /**< Similarity score [0,1] */
        char description[256];              /**< Pattern description */
        char suggested_fix[512];            /**< Previously successful fix */
    } similar_patterns[MAX_FAILURE_PATTERNS];
    uint32_t pattern_count;                 /**< Number of matching patterns */

    /* Overall assessment */
    float repair_difficulty;                /**< Estimated repair difficulty [0,1] */
    float confidence;                       /**< Analysis confidence [0,1] */
    char root_cause_hypothesis[512];        /**< Hypothesized root cause */
    char recommended_approach[512];         /**< Recommended repair approach */
} code_analysis_result_t;

/**
 * @brief Recovery strategy enhancement from parietal
 */
typedef struct {
    /* Mathematical assessment */
    float estimated_success_rate;           /**< Estimated success probability */
    float estimated_recovery_time_ms;       /**< Estimated time to recover */
    float resource_requirement;             /**< Resource requirement [0,1] */

    /* Spatial understanding */
    uint32_t impacted_components;           /**< Number of impacted components */
    char critical_path[256];                /**< Critical path description */

    /* Strategy recommendations */
    recovery_goal_t recommended_goal;       /**< Recommended recovery goal */
    recovery_exec_action_t priority_actions[5]; /**< Priority actions */
    uint32_t priority_action_count;         /**< Number of priority actions */

    /* Risk assessment */
    float risk_of_cascade;                  /**< Risk of cascading failures */
    float risk_of_data_loss;                /**< Risk of data loss */
    char risk_mitigation[256];              /**< Risk mitigation suggestion */

    /* Learning integration */
    bool should_update_patterns;            /**< Should update failure patterns */
    char pattern_update[256];               /**< Pattern update description */
} recovery_enhancement_t;

/**
 * @brief Parietal-recovery bridge configuration
 */
typedef struct {
    bool enable_code_analysis;              /**< Enable code structure analysis */
    bool enable_pattern_matching;           /**< Enable historical pattern matching */
    bool enable_spatial_reasoning;          /**< Enable spatial code analysis */
    bool enable_complexity_estimation;      /**< Enable complexity estimation */
    bool enable_learning;                   /**< Enable pattern learning from outcomes */
    uint32_t max_analysis_time_ms;          /**< Max time for analysis */
    float min_pattern_similarity;           /**< Min similarity for pattern match */
    uint32_t max_dependency_depth;          /**< Max dependency trace depth */
} recovery_parietal_config_t;

/**
 * @brief Parietal-recovery bridge statistics
 */
typedef struct {
    uint64_t total_analyses;                /**< Total code analyses performed */
    uint64_t patterns_matched;              /**< Patterns matched to history */
    uint64_t patterns_learned;              /**< New patterns learned */
    uint64_t enhancements_provided;         /**< Recovery enhancements provided */
    float avg_analysis_time_ms;             /**< Average analysis time */
    float avg_repair_difficulty;            /**< Average repair difficulty */
    uint64_t successful_predictions;        /**< Successful outcome predictions */
    uint64_t failed_predictions;            /**< Failed outcome predictions */
} recovery_parietal_stats_t;

/**
 * @brief Opaque handle for parietal-recovery bridge
 */
typedef struct recovery_parietal_bridge recovery_parietal_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @return Default configuration
 */
recovery_parietal_config_t recovery_parietal_default_config(void);

/**
 * @brief Create parietal-recovery bridge
 *
 * @param parietal Parietal lobe handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
recovery_parietal_bridge_t* recovery_parietal_bridge_create(
    parietal_lobe_t* parietal,
    const recovery_parietal_config_t* config
);

/**
 * @brief Destroy parietal-recovery bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void recovery_parietal_bridge_destroy(recovery_parietal_bridge_t* bridge);

/**
 * @brief Check if bridge is ready
 *
 * @param bridge Bridge handle
 * @return true if ready
 */
bool recovery_parietal_bridge_is_ready(const recovery_parietal_bridge_t* bridge);

//=============================================================================
// Recovery Executive Attachment Functions
//=============================================================================

/**
 * @brief Attach bridge to recovery executive
 *
 * @param bridge Bridge handle
 * @param exec Recovery executive handle
 * @return 0 on success
 */
int recovery_parietal_bridge_attach_executive(
    recovery_parietal_bridge_t* bridge,
    recovery_executive_t* exec
);

/**
 * @brief Attach parietal to recovery executive (convenience)
 *
 * Creates bridge internally and attaches to executive.
 *
 * @param exec Recovery executive handle
 * @param parietal Parietal lobe handle
 * @return 0 on success
 */
int recovery_executive_attach_parietal(
    recovery_executive_t* exec,
    parietal_lobe_t* parietal
);

/**
 * @brief Get parietal from recovery executive
 *
 * @param exec Recovery executive handle
 * @return Parietal handle or NULL if not attached
 */
parietal_lobe_t* recovery_executive_get_parietal(
    const recovery_executive_t* exec
);

//=============================================================================
// Code Analysis Functions
//=============================================================================

/**
 * @brief Analyze code at failure location
 *
 * Uses parietal's software engineering module to analyze code structure.
 *
 * @param bridge Bridge handle
 * @param request Analysis request
 * @param result Output analysis result
 * @return 0 on success
 */
int recovery_parietal_analyze_code(
    recovery_parietal_bridge_t* bridge,
    const code_analysis_request_t* request,
    code_analysis_result_t* result
);

/**
 * @brief Analyze dependency impact
 *
 * Uses parietal's spatial reasoning for dependency graph analysis.
 *
 * @param bridge Bridge handle
 * @param location Failure location
 * @param depth Maximum trace depth
 * @param affected_modules Output affected module names
 * @param max_modules Maximum modules to return
 * @return Number of affected modules
 */
int recovery_parietal_analyze_impact(
    recovery_parietal_bridge_t* bridge,
    const code_location_t* location,
    uint32_t depth,
    char affected_modules[][128],
    uint32_t max_modules
);

/**
 * @brief Detect code smells at location
 *
 * @param bridge Bridge handle
 * @param location Code location
 * @param smells Output smell results
 * @param max_smells Maximum smells to return
 * @return Number of smells detected
 */
int recovery_parietal_detect_smells(
    recovery_parietal_bridge_t* bridge,
    const code_location_t* location,
    smell_result_t* smells,
    uint32_t max_smells
);

//=============================================================================
// Pattern Matching Functions
//=============================================================================

/**
 * @brief Find similar failure patterns in history
 *
 * Uses parietal's pattern detection for historical matching.
 *
 * @param bridge Bridge handle
 * @param diagnosis Current failure diagnosis
 * @param location Failure location
 * @param patterns Output matching patterns
 * @param max_patterns Maximum patterns to return
 * @return Number of matching patterns
 */
int recovery_parietal_find_similar_failures(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_location_t* location,
    code_analysis_result_t* result,
    uint32_t max_patterns
);

/**
 * @brief Learn pattern from recovery outcome
 *
 * Updates pattern database based on recovery success/failure.
 *
 * @param bridge Bridge handle
 * @param diagnosis Original diagnosis
 * @param location Failure location
 * @param plan Plan that was executed
 * @param success Whether recovery succeeded
 * @return 0 on success
 */
int recovery_parietal_learn_pattern(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_location_t* location,
    const recovery_plan_t* plan,
    bool success
);

//=============================================================================
// Recovery Enhancement Functions
//=============================================================================

/**
 * @brief Enhance recovery plan with parietal analysis
 *
 * Uses parietal capabilities to improve recovery planning.
 *
 * @param bridge Bridge handle
 * @param exec Recovery executive
 * @param diagnosis Failure diagnosis
 * @param location Failure location
 * @param enhancement Output enhancement recommendations
 * @return 0 on success
 */
int recovery_parietal_enhance_plan(
    recovery_parietal_bridge_t* bridge,
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    const code_location_t* location,
    recovery_enhancement_t* enhancement
);

/**
 * @brief Create enhanced recovery plan
 *
 * Creates plan using both executive and parietal capabilities.
 *
 * @param bridge Bridge handle
 * @param exec Recovery executive
 * @param diagnosis Failure diagnosis
 * @param goal Recovery goal
 * @param location Optional failure location
 * @return Enhanced recovery plan or NULL on failure
 */
recovery_plan_t* recovery_parietal_create_enhanced_plan(
    recovery_parietal_bridge_t* bridge,
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    recovery_goal_t goal,
    const code_location_t* location
);

/**
 * @brief Estimate recovery success probability
 *
 * Uses parietal's mathematical intuition for estimation.
 *
 * @param bridge Bridge handle
 * @param plan Recovery plan
 * @param diagnosis Failure diagnosis
 * @return Estimated success probability [0,1]
 */
float recovery_parietal_estimate_success(
    recovery_parietal_bridge_t* bridge,
    const recovery_plan_t* plan,
    const diagnostic_result_t* diagnosis
);

//=============================================================================
// Hypothesis Testing Functions
//=============================================================================

/**
 * @brief Create recovery hypothesis
 *
 * Uses parietal's scientific reasoning for hypothesis creation.
 *
 * @param bridge Bridge handle
 * @param diagnosis Failure diagnosis
 * @param hypothesis Output hypothesis description
 * @param max_len Maximum hypothesis length
 * @param confidence Output confidence in hypothesis
 * @return 0 on success
 */
int recovery_parietal_create_hypothesis(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    char* hypothesis,
    uint32_t max_len,
    float* confidence
);

/**
 * @brief Test recovery hypothesis
 *
 * Uses parietal's hypothesis testing to verify recovery approach.
 *
 * @param bridge Bridge handle
 * @param hypothesis Hypothesis to test
 * @param evidence Evidence from recovery attempt
 * @param updated_confidence Output updated confidence
 * @return 0 on success
 */
int recovery_parietal_test_hypothesis(
    recovery_parietal_bridge_t* bridge,
    const char* hypothesis,
    const recovery_execution_result_t* evidence,
    float* updated_confidence
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success
 */
int recovery_parietal_get_stats(
    const recovery_parietal_bridge_t* bridge,
    recovery_parietal_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 */
void recovery_parietal_reset_stats(recovery_parietal_bridge_t* bridge);

//=============================================================================
// Code Generation Integration (Self-Repair Pipeline)
//=============================================================================

/**
 * @brief Generate code fix for diagnosed error
 *
 * WHAT: Generate code fix using parietal software engineering module
 * WHY:  Enable autonomous code generation for self-repair
 * HOW:  Analyze code → select strategy → generate fix candidates → return best
 *
 * This function integrates with the code generation engine to produce
 * fixes that can be deployed via the self-repair coordinator.
 *
 * @param bridge Bridge handle
 * @param diagnosis Diagnostic result from fault tolerance
 * @param analysis Code analysis result (can be NULL, will analyze if needed)
 * @param fix_code Output buffer for generated fix code
 * @param fix_code_size Size of fix_code buffer
 * @param fix_confidence Output confidence score [0,1]
 * @param fix_explanation Output explanation buffer
 * @param explanation_size Size of explanation buffer
 * @return 0 on success, -1 on failure
 */
int recovery_parietal_generate_fix(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_analysis_result_t* analysis,
    char* fix_code,
    size_t fix_code_size,
    float* fix_confidence,
    char* fix_explanation,
    size_t explanation_size
);

/**
 * @brief Generate multiple fix candidates for diagnosed error
 *
 * WHAT: Generate multiple fix candidates with different strategies
 * WHY:  Provide options for the self-repair coordinator to choose from
 * HOW:  Generate fixes using multiple compatible strategies
 *
 * @param bridge Bridge handle
 * @param diagnosis Diagnostic result
 * @param analysis Code analysis result (can be NULL)
 * @param candidates Output array of fix candidates (caller allocated)
 * @param max_candidates Maximum candidates to generate
 * @param generated_count Output: number of candidates generated
 * @return 0 on success
 */
int recovery_parietal_generate_fix_candidates(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_analysis_result_t* analysis,
    void* candidates,
    uint32_t max_candidates,
    uint32_t* generated_count
);

/**
 * @brief Learn from fix outcome for future generation
 *
 * WHAT: Update pattern database based on fix success/failure
 * WHY:  Improve future fix generation accuracy
 * HOW:  Record fix details and outcome in learning history
 *
 * @param bridge Bridge handle
 * @param diagnosis Original diagnosis
 * @param fix_code The fix that was applied
 * @param strategy Strategy used
 * @param success Whether fix succeeded
 * @return 0 on success
 */
int recovery_parietal_learn_fix_outcome(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const char* fix_code,
    uint32_t strategy,
    bool success
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Free code analysis result resources
 *
 * @param result Result to free (NULL safe)
 */
void recovery_parietal_free_analysis_result(code_analysis_result_t* result);

/**
 * @brief Get bridge version string
 *
 * @return Version string
 */
const char* recovery_parietal_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RECOVERY_PARIETAL_BRIDGE_H */
