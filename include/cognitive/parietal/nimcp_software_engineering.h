/**
 * @file nimcp_software_engineering.h
 * @brief Software engineering reasoning module for parietal lobe
 *
 * Implements software engineering reasoning capabilities:
 * - Algorithm complexity analysis (Big-O notation)
 * - Design pattern recognition
 * - Code structure analysis
 * - Dependency graph analysis
 * - Software metrics calculation
 *
 * BIOLOGICAL BASIS:
 * Software engineering reasoning combines mathematical intuition
 * (for complexity analysis), pattern detection (for design patterns),
 * and spatial reasoning (for code architecture visualization).
 *
 * USAGE:
 * ```c
 * software_eng_t* se = software_eng_create();
 *
 * // Analyze algorithm complexity
 * complexity_t complexity = software_eng_analyze_complexity(se, algorithm);
 *
 * // Detect design pattern
 * design_pattern_t pattern = software_eng_detect_pattern(se, code_structure);
 *
 * software_eng_destroy(se);
 * ```
 */

#ifndef NIMCP_SOFTWARE_ENGINEERING_H
#define NIMCP_SOFTWARE_ENGINEERING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum identifier length */
#define SWENG_MAX_IDENTIFIER            128

/** Maximum number of nodes in dependency graph */
#define SWENG_MAX_GRAPH_NODES           256

/** Maximum number of metrics */
#define SWENG_MAX_METRICS               32

/** Bio-async module ID for software engineering */
#define BIO_MODULE_SOFTWARE_ENG         0x0387

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for software engineering processor */
typedef struct software_eng software_eng_t;

/**
 * @brief Big-O complexity classes
 */
typedef enum {
    COMPLEXITY_O_1,             /**< O(1) - Constant */
    COMPLEXITY_O_LOG_N,         /**< O(log n) - Logarithmic */
    COMPLEXITY_O_N,             /**< O(n) - Linear */
    COMPLEXITY_O_N_LOG_N,       /**< O(n log n) - Linearithmic */
    COMPLEXITY_O_N_SQUARED,     /**< O(n^2) - Quadratic */
    COMPLEXITY_O_N_CUBED,       /**< O(n^3) - Cubic */
    COMPLEXITY_O_2_N,           /**< O(2^n) - Exponential */
    COMPLEXITY_O_N_FACTORIAL,   /**< O(n!) - Factorial */
    COMPLEXITY_UNKNOWN          /**< Unknown complexity */
} complexity_class_t;

/**
 * @brief Design pattern categories
 */
typedef enum {
    PATTERN_CATEGORY_CREATIONAL,
    PATTERN_CATEGORY_STRUCTURAL,
    PATTERN_CATEGORY_BEHAVIORAL,
    PATTERN_CATEGORY_UNKNOWN
} pattern_category_t;

/**
 * @brief Design pattern types
 */
typedef enum {
    /* Creational */
    DESIGN_PATTERN_SINGLETON,
    DESIGN_PATTERN_FACTORY,
    DESIGN_PATTERN_ABSTRACT_FACTORY,
    DESIGN_PATTERN_BUILDER,
    DESIGN_PATTERN_PROTOTYPE,
    /* Structural */
    DESIGN_PATTERN_ADAPTER,
    DESIGN_PATTERN_BRIDGE,
    DESIGN_PATTERN_COMPOSITE,
    DESIGN_PATTERN_DECORATOR,
    DESIGN_PATTERN_FACADE,
    DESIGN_PATTERN_FLYWEIGHT,
    DESIGN_PATTERN_PROXY,
    /* Behavioral */
    DESIGN_PATTERN_CHAIN_OF_RESPONSIBILITY,
    DESIGN_PATTERN_COMMAND,
    DESIGN_PATTERN_ITERATOR,
    DESIGN_PATTERN_MEDIATOR,
    DESIGN_PATTERN_MEMENTO,
    DESIGN_PATTERN_OBSERVER,
    DESIGN_PATTERN_STATE,
    DESIGN_PATTERN_STRATEGY,
    DESIGN_PATTERN_TEMPLATE_METHOD,
    DESIGN_PATTERN_VISITOR,
    /* Unknown */
    DESIGN_PATTERN_NONE
} design_pattern_t;

/**
 * @brief Code smell types
 */
typedef enum {
    SMELL_LONG_METHOD,
    SMELL_LARGE_CLASS,
    SMELL_FEATURE_ENVY,
    SMELL_DATA_CLUMPS,
    SMELL_PRIMITIVE_OBSESSION,
    SMELL_SWITCH_STATEMENTS,
    SMELL_PARALLEL_INHERITANCE,
    SMELL_LAZY_CLASS,
    SMELL_SPECULATIVE_GENERALITY,
    SMELL_DEAD_CODE,
    SMELL_DUPLICATE_CODE,
    SMELL_GOD_CLASS,
    SMELL_NONE
} code_smell_t;

/**
 * @brief Complexity analysis result
 */
typedef struct {
    complexity_class_t time_complexity;     /**< Time complexity class */
    complexity_class_t space_complexity;    /**< Space complexity class */
    float time_coefficient;                 /**< Estimated coefficient */
    float space_coefficient;                /**< Space coefficient */
    float confidence;                       /**< Confidence in analysis [0,1] */
    char description[256];                  /**< Human-readable description */
} complexity_result_t;

/**
 * @brief Algorithm characteristics for complexity analysis
 */
typedef struct {
    bool has_loops;                         /**< Contains loops */
    uint32_t loop_depth;                    /**< Maximum nested loop depth */
    bool has_recursion;                     /**< Uses recursion */
    uint32_t recursive_calls;               /**< Number of recursive calls */
    bool has_divide_conquer;                /**< Uses divide and conquer */
    bool has_dynamic_programming;           /**< Uses dynamic programming */
    bool has_sorting;                       /**< Contains sorting */
    bool has_searching;                     /**< Contains searching */
    uint32_t input_size_n;                  /**< Primary input size parameter */
    uint32_t auxiliary_space;               /**< Auxiliary space used */
} algorithm_traits_t;

/**
 * @brief Code structure for pattern detection
 */
typedef struct {
    uint32_t num_classes;                   /**< Number of classes/modules */
    uint32_t num_interfaces;                /**< Number of interfaces */
    uint32_t num_methods;                   /**< Total methods */
    bool has_single_instance;               /**< Singleton-like behavior */
    bool has_factory_method;                /**< Factory method present */
    bool has_abstract_base;                 /**< Abstract base class */
    bool has_composition;                   /**< Uses composition */
    bool has_inheritance;                   /**< Uses inheritance */
    bool has_callbacks;                     /**< Uses callbacks/observers */
    bool has_strategy_interface;            /**< Swappable strategies */
    float coupling_score;                   /**< Coupling metric [0,1] */
    float cohesion_score;                   /**< Cohesion metric [0,1] */
} code_structure_t;

/**
 * @brief Pattern detection result
 */
typedef struct {
    design_pattern_t pattern;               /**< Detected pattern */
    pattern_category_t category;            /**< Pattern category */
    float confidence;                       /**< Detection confidence [0,1] */
    char description[256];                  /**< Pattern description */
    char recommendation[256];               /**< Usage recommendation */
} pattern_result_t;

/**
 * @brief Dependency graph node
 */
typedef struct {
    uint32_t id;                            /**< Node ID */
    char name[SWENG_MAX_IDENTIFIER];        /**< Module/class name */
    uint32_t dependencies[SWENG_MAX_GRAPH_NODES]; /**< Dependency IDs */
    uint32_t num_dependencies;              /**< Number of dependencies */
    uint32_t dependents_count;              /**< How many depend on this */
} dep_node_t;

/**
 * @brief Dependency graph
 */
typedef struct {
    dep_node_t nodes[SWENG_MAX_GRAPH_NODES]; /**< Graph nodes */
    uint32_t num_nodes;                     /**< Number of nodes */
    bool has_cycles;                        /**< Contains circular dependencies */
    uint32_t max_depth;                     /**< Maximum dependency depth */
    float stability_avg;                    /**< Average stability metric */
} dep_graph_t;

/**
 * @brief Software metrics
 */
typedef struct {
    /* Size metrics */
    uint32_t lines_of_code;                 /**< Total LOC */
    uint32_t num_functions;                 /**< Number of functions */
    uint32_t num_classes;                   /**< Number of classes */
    uint32_t num_modules;                   /**< Number of modules */

    /* Complexity metrics */
    float cyclomatic_complexity;            /**< McCabe's cyclomatic complexity */
    float cognitive_complexity;             /**< Cognitive complexity */
    float halstead_difficulty;              /**< Halstead difficulty */
    float halstead_effort;                  /**< Halstead effort */

    /* Coupling/Cohesion */
    float afferent_coupling;                /**< Ca - incoming dependencies */
    float efferent_coupling;                /**< Ce - outgoing dependencies */
    float instability;                      /**< I = Ce / (Ca + Ce) */
    float abstractness;                     /**< A = abstract / total */

    /* Quality metrics */
    float maintainability_index;            /**< Maintainability index */
    float test_coverage;                    /**< Test coverage percentage */
    float documentation_ratio;              /**< Documentation ratio */
} software_metrics_t;

/**
 * @brief Code smell detection result
 */
typedef struct {
    code_smell_t smell;                     /**< Detected smell */
    float severity;                         /**< Severity [0,1] */
    char location[SWENG_MAX_IDENTIFIER];    /**< Location in code */
    char suggestion[256];                   /**< Refactoring suggestion */
} smell_result_t;

/**
 * @brief Software engineering configuration
 */
typedef struct {
    float complexity_threshold;             /**< Threshold for high complexity */
    float coupling_threshold;               /**< Threshold for high coupling */
    uint32_t method_length_limit;           /**< Max method length */
    uint32_t class_size_limit;              /**< Max class size */
    bool strict_mode;                       /**< Strict analysis mode */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    float inflammation_sensitivity;         /**< Immune modulation sensitivity */
    float sleep_deprivation_factor;         /**< Sleep modulation factor */
} software_eng_config_t;

/**
 * @brief Software engineering statistics
 */
typedef struct {
    uint64_t complexity_analyses;           /**< Complexity analyses performed */
    uint64_t pattern_detections;            /**< Pattern detections */
    uint64_t smell_detections;              /**< Smell detections */
    uint64_t metric_calculations;           /**< Metric calculations */
    float avg_processing_time_us;           /**< Average processing time */
    /* Intuitive reasoning stats */
    uint64_t hunches_generated;             /**< Hunches/intuitions generated */
    uint64_t hunches_validated;             /**< Hunches confirmed correct */
    uint64_t extrapolations_made;           /**< Extrapolations performed */
    float hunch_accuracy;                   /**< Historical hunch accuracy [0,1] */
} software_eng_stats_t;

/* ============================================================================
 * INTUITIVE REASONING TYPES
 * ============================================================================ */

/**
 * @brief Types of software engineering hunches
 */
typedef enum {
    SE_HUNCH_COMPLEXITY_GROWTH,             /**< Code complexity will grow */
    SE_HUNCH_TECH_DEBT_ACCUMULATION,        /**< Technical debt accumulating */
    SE_HUNCH_PATTERN_EMERGING,              /**< Design pattern trying to emerge */
    SE_HUNCH_REFACTOR_NEEDED,               /**< Refactoring opportunity detected */
    SE_HUNCH_DEPENDENCY_RISK,               /**< Dependency becoming risky */
    SE_HUNCH_PERFORMANCE_DEGRADATION,       /**< Performance will degrade */
    SE_HUNCH_MAINTAINABILITY_DECLINE,       /**< Maintainability declining */
    SE_HUNCH_ARCHITECTURE_SMELL,            /**< Architectural issue forming */
    SE_HUNCH_SECURITY_CONCERN,              /**< Security issue intuited */
    SE_HUNCH_POSITIVE_TREND,                /**< Code quality improving */
    SE_HUNCH_UNKNOWN                        /**< Unclassified hunch */
} se_hunch_type_t;

/**
 * @brief Confidence level for intuitive reasoning
 */
typedef enum {
    SE_CONFIDENCE_VERY_LOW,                 /**< < 20% certainty - weak signal */
    SE_CONFIDENCE_LOW,                      /**< 20-40% certainty - possible */
    SE_CONFIDENCE_MODERATE,                 /**< 40-60% certainty - likely */
    SE_CONFIDENCE_HIGH,                     /**< 60-80% certainty - probable */
    SE_CONFIDENCE_VERY_HIGH                 /**< > 80% certainty - near certain */
} se_confidence_level_t;

/**
 * @brief Extrapolation trend direction
 */
typedef enum {
    SE_TREND_IMPROVING,                     /**< Getting better */
    SE_TREND_STABLE,                        /**< No significant change */
    SE_TREND_DECLINING,                     /**< Getting worse */
    SE_TREND_VOLATILE,                      /**< Unpredictable swings */
    SE_TREND_UNKNOWN                        /**< Cannot determine */
} se_trend_t;

/**
 * @brief Software engineering hunch/intuition
 */
typedef struct {
    se_hunch_type_t type;                   /**< Type of hunch */
    float plausibility;                     /**< How reasonable [0,1] */
    float urgency;                          /**< How urgent to address [0,1] */
    float novelty;                          /**< How unexpected [0,1] */
    se_confidence_level_t confidence;       /**< Confidence in hunch */
    char description[256];                  /**< Human-readable description */
    char reasoning[512];                    /**< Why this hunch was formed */
    char suggestion[256];                   /**< Suggested action */
    uint64_t timestamp;                     /**< When hunch was formed */
} se_hunch_t;

/**
 * @brief Software metric data point for extrapolation
 */
typedef struct {
    uint64_t timestamp;                     /**< When measured */
    float value;                            /**< Metric value */
    float confidence;                       /**< Measurement confidence [0,1] */
} se_metric_point_t;

/**
 * @brief Metric time series for extrapolation
 */
typedef struct {
    se_metric_point_t* points;              /**< Array of data points */
    uint32_t num_points;                    /**< Number of points */
    uint32_t capacity;                      /**< Allocated capacity */
    char metric_name[64];                   /**< Name of the metric */
} se_metric_series_t;

/**
 * @brief Extrapolation result
 */
typedef struct {
    se_metric_point_t** predicted;          /**< Predicted future values */
    uint32_t num_predicted;                 /**< Number of predictions */
    se_trend_t trend;                       /**< Overall trend */
    float trend_slope;                      /**< Rate of change */
    float r_squared;                        /**< Fit quality [0,1] */
    float extrapolation_confidence;         /**< Confidence in predictions [0,1] */
    uint64_t validity_horizon;              /**< How far predictions are valid */
    char interpretation[256];               /**< Human-readable interpretation */
} se_extrapolation_t;

/**
 * @brief Software insight from pattern recognition
 */
typedef struct {
    char title[128];                        /**< Insight title */
    char description[512];                  /**< Detailed description */
    float importance;                       /**< How important [0,1] */
    float actionability;                    /**< How actionable [0,1] */
    char action_items[3][128];              /**< Suggested actions */
    uint32_t num_actions;                   /**< Number of action items */
    se_confidence_level_t confidence;       /**< Confidence level */
    char* related_code_paths;               /**< Affected code paths (comma-separated) */
} se_insight_t;

/**
 * @brief Analogy between codebases or patterns
 */
typedef struct {
    char source_domain[128];                /**< Known/source pattern */
    char target_domain[128];                /**< New/target situation */
    char mapping[512];                      /**< How they correspond */
    float mapping_strength;                 /**< Quality of analogy [0,1] */
    char transferred_insight[256];          /**< Knowledge transferred */
    char caveats[256];                      /**< Where analogy breaks down */
} se_analogy_t;

/**
 * @brief Knowledge synthesis result
 */
typedef struct {
    se_insight_t* insights;                 /**< Synthesized insights */
    uint32_t num_insights;                  /**< Number of insights */
    char* knowledge_gaps;                   /**< Identified gaps (comma-separated) */
    char* contradictions;                   /**< Found contradictions */
    float coherence;                        /**< How well pieces fit [0,1] */
    char summary[512];                      /**< Overall synthesis summary */
} se_synthesis_t;

/**
 * @brief Configuration for intuitive reasoning
 */
typedef struct {
    float hunch_threshold;                  /**< Min plausibility to report [0,1] */
    uint32_t max_hunches;                   /**< Max hunches to track */
    uint32_t extrapolation_horizon;         /**< How far to extrapolate (days) */
    float extrapolation_decay;              /**< Confidence decay rate per step */
    bool enable_analogical_reasoning;       /**< Use analogy for transfer */
    bool enable_synthesis;                  /**< Synthesize knowledge */
    uint32_t min_data_points;               /**< Min points for extrapolation */
    float novelty_threshold;                /**< Min novelty to report [0,1] */
} se_intuition_config_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create software engineering processor with default configuration
 *
 * @return Handle or NULL on error
 */
software_eng_t* software_eng_create(void);

/**
 * @brief Create software engineering processor with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
software_eng_t* software_eng_create_custom(const software_eng_config_t* config);

/**
 * @brief Destroy software engineering processor
 *
 * @param se Handle (NULL safe)
 */
void software_eng_destroy(software_eng_t* se);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
software_eng_config_t software_eng_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool software_eng_validate_config(const software_eng_config_t* config);

/* ============================================================================
 * COMPLEXITY ANALYSIS API
 * ============================================================================ */

/**
 * @brief Analyze algorithm complexity from traits
 *
 * @param se Software engineering handle
 * @param traits Algorithm characteristics
 * @param result Output complexity result
 * @return 0 on success
 */
int software_eng_analyze_complexity(
    software_eng_t* se,
    const algorithm_traits_t* traits,
    complexity_result_t* result
);

/**
 * @brief Get complexity class description
 *
 * @param complexity Complexity class
 * @return Human-readable description
 */
const char* software_eng_complexity_to_string(complexity_class_t complexity);

/**
 * @brief Compare two complexity classes
 *
 * @param a First complexity
 * @param b Second complexity
 * @return -1 if a < b, 0 if equal, 1 if a > b
 */
int software_eng_compare_complexity(
    complexity_class_t a,
    complexity_class_t b
);

/**
 * @brief Estimate runtime for given input size
 *
 * @param se Software engineering handle
 * @param complexity Complexity class
 * @param coefficient Base coefficient
 * @param input_size Input size n
 * @return Estimated operations count
 */
double software_eng_estimate_runtime(
    const software_eng_t* se,
    complexity_class_t complexity,
    double coefficient,
    uint64_t input_size
);

/* ============================================================================
 * PATTERN DETECTION API
 * ============================================================================ */

/**
 * @brief Detect design pattern from code structure
 *
 * @param se Software engineering handle
 * @param structure Code structure description
 * @param result Output pattern result
 * @return 0 on success
 */
int software_eng_detect_pattern(
    software_eng_t* se,
    const code_structure_t* structure,
    pattern_result_t* result
);

/**
 * @brief Get pattern type name
 *
 * @param pattern Pattern type
 * @return Pattern name string
 */
const char* software_eng_pattern_to_string(design_pattern_t pattern);

/**
 * @brief Get pattern category name
 *
 * @param category Pattern category
 * @return Category name string
 */
const char* software_eng_category_to_string(pattern_category_t category);

/**
 * @brief Check if pattern is applicable to structure
 *
 * @param se Software engineering handle
 * @param pattern Pattern to check
 * @param structure Code structure
 * @return Applicability score [0,1]
 */
float software_eng_pattern_applicability(
    const software_eng_t* se,
    design_pattern_t pattern,
    const code_structure_t* structure
);

/* ============================================================================
 * CODE SMELL DETECTION API
 * ============================================================================ */

/**
 * @brief Detect code smells in metrics
 *
 * @param se Software engineering handle
 * @param metrics Software metrics
 * @param results Output smell results (array)
 * @param max_results Maximum results to return
 * @return Number of smells detected
 */
uint32_t software_eng_detect_smells(
    software_eng_t* se,
    const software_metrics_t* metrics,
    smell_result_t* results,
    uint32_t max_results
);

/**
 * @brief Get smell name
 *
 * @param smell Code smell type
 * @return Smell name string
 */
const char* software_eng_smell_to_string(code_smell_t smell);

/* ============================================================================
 * DEPENDENCY ANALYSIS API
 * ============================================================================ */

/**
 * @brief Initialize dependency graph
 *
 * @param graph Graph to initialize
 */
void software_eng_init_graph(dep_graph_t* graph);

/**
 * @brief Add node to dependency graph
 *
 * @param graph Dependency graph
 * @param name Node name
 * @return Node ID or -1 on error
 */
int software_eng_add_node(dep_graph_t* graph, const char* name);

/**
 * @brief Add dependency edge
 *
 * @param graph Dependency graph
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @return 0 on success
 */
int software_eng_add_dependency(dep_graph_t* graph, uint32_t from_id, uint32_t to_id);

/**
 * @brief Detect cycles in dependency graph
 *
 * @param se Software engineering handle
 * @param graph Dependency graph
 * @return true if cycles exist
 */
bool software_eng_detect_cycles(software_eng_t* se, dep_graph_t* graph);

/**
 * @brief Calculate dependency metrics
 *
 * @param se Software engineering handle
 * @param graph Dependency graph
 * @return 0 on success
 */
int software_eng_analyze_dependencies(software_eng_t* se, dep_graph_t* graph);

/**
 * @brief Get topological sort order
 *
 * @param se Software engineering handle
 * @param graph Dependency graph
 * @param order Output order array (node IDs)
 * @param max_order Maximum order entries
 * @return Number of nodes in order, or -1 if cycles
 */
int software_eng_topological_sort(
    software_eng_t* se,
    const dep_graph_t* graph,
    uint32_t* order,
    uint32_t max_order
);

/* ============================================================================
 * METRICS API
 * ============================================================================ */

/**
 * @brief Calculate cyclomatic complexity
 *
 * @param edges Number of edges in control flow graph
 * @param nodes Number of nodes
 * @param components Number of connected components
 * @return Cyclomatic complexity
 */
float software_eng_cyclomatic_complexity(
    uint32_t edges,
    uint32_t nodes,
    uint32_t components
);

/**
 * @brief Calculate maintainability index
 *
 * @param halstead_volume Halstead volume
 * @param cyclomatic_complexity Cyclomatic complexity
 * @param loc Lines of code
 * @return Maintainability index [0, 100]
 */
float software_eng_maintainability_index(
    float halstead_volume,
    float cyclomatic_complexity,
    uint32_t loc
);

/**
 * @brief Calculate instability metric
 *
 * @param afferent_coupling Ca (incoming dependencies)
 * @param efferent_coupling Ce (outgoing dependencies)
 * @return Instability [0, 1]
 */
float software_eng_instability(float afferent_coupling, float efferent_coupling);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * @param se Software engineering handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int software_eng_set_inflammation(software_eng_t* se, float level);

/**
 * @brief Set sleep deprivation level
 *
 * @param se Software engineering handle
 * @param level Deprivation level [0,1]
 * @return 0 on success
 */
int software_eng_set_sleep_deprivation(software_eng_t* se, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param se Software engineering handle
 * @param stats Output statistics
 * @return 0 on success
 */
int software_eng_get_stats(software_eng_t* se, software_eng_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param se Software engineering handle
 */
void software_eng_reset_stats(software_eng_t* se);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* software_eng_get_last_error(void);

/* ============================================================================
 * INTUITIVE REASONING API
 * ============================================================================ */

/**
 * @brief Get default intuition configuration
 *
 * @return Default intuition config
 */
se_intuition_config_t software_eng_intuition_default_config(void);

/**
 * @brief Enable intuitive reasoning mode
 *
 * @param se Software engineering handle
 * @param config Intuition configuration (NULL for defaults)
 * @return 0 on success
 */
int software_eng_enable_intuition(software_eng_t* se, const se_intuition_config_t* config);

/**
 * @brief Disable intuitive reasoning mode
 *
 * @param se Software engineering handle
 * @return 0 on success
 */
int software_eng_disable_intuition(software_eng_t* se);

/* ============================================================================
 * HUNCH GENERATION API
 * ============================================================================ */

/**
 * @brief Form a hunch about code quality from metrics
 *
 * Uses pattern recognition and past experience to form an intuitive
 * assessment that goes beyond raw metric analysis.
 *
 * @param se Software engineering handle
 * @param metrics Current software metrics
 * @param history Optional historical metrics (NULL if not available)
 * @param history_count Number of historical entries
 * @param hunch Output hunch
 * @return 0 on success
 */
int software_eng_form_hunch(
    software_eng_t* se,
    const software_metrics_t* metrics,
    const software_metrics_t* history,
    uint32_t history_count,
    se_hunch_t* hunch
);

/**
 * @brief Form multiple hunches from comprehensive analysis
 *
 * @param se Software engineering handle
 * @param metrics Current metrics
 * @param structure Code structure
 * @param graph Dependency graph (optional)
 * @param hunches Output hunches array
 * @param max_hunches Maximum hunches to generate
 * @return Number of hunches generated, -1 on error
 */
int software_eng_form_hunches(
    software_eng_t* se,
    const software_metrics_t* metrics,
    const code_structure_t* structure,
    const dep_graph_t* graph,
    se_hunch_t* hunches,
    uint32_t max_hunches
);

/**
 * @brief Validate a hunch against actual outcome
 *
 * Feeds back whether a hunch was correct to improve future intuitions.
 *
 * @param se Software engineering handle
 * @param hunch The hunch that was made
 * @param was_correct Whether the hunch turned out to be correct
 * @param actual_outcome Description of what actually happened
 * @return 0 on success
 */
int software_eng_validate_hunch(
    software_eng_t* se,
    const se_hunch_t* hunch,
    bool was_correct,
    const char* actual_outcome
);

/**
 * @brief Get hunch type name
 *
 * @param type Hunch type
 * @return Human-readable name
 */
const char* software_eng_hunch_type_to_string(se_hunch_type_t type);

/**
 * @brief Get confidence level name
 *
 * @param level Confidence level
 * @return Human-readable name
 */
const char* software_eng_confidence_to_string(se_confidence_level_t level);

/* ============================================================================
 * EXTRAPOLATION API
 * ============================================================================ */

/**
 * @brief Create metric time series
 *
 * @param metric_name Name of the metric
 * @param initial_capacity Initial capacity
 * @return Time series handle, or NULL on error
 */
se_metric_series_t* software_eng_create_series(const char* metric_name, uint32_t initial_capacity);

/**
 * @brief Destroy metric time series
 *
 * @param series Series to destroy
 */
void software_eng_destroy_series(se_metric_series_t* series);

/**
 * @brief Add data point to time series
 *
 * @param series Time series
 * @param timestamp Timestamp
 * @param value Metric value
 * @param confidence Measurement confidence [0,1]
 * @return 0 on success
 */
int software_eng_add_data_point(
    se_metric_series_t* series,
    uint64_t timestamp,
    float value,
    float confidence
);

/**
 * @brief Extrapolate metric values into the future
 *
 * Uses trend analysis and learned patterns to predict future metric values.
 * Confidence decreases with prediction distance.
 *
 * @param se Software engineering handle
 * @param series Historical data points
 * @param horizon_steps How many steps to predict
 * @param result Output extrapolation result
 * @return 0 on success
 */
int software_eng_extrapolate(
    software_eng_t* se,
    const se_metric_series_t* series,
    uint32_t horizon_steps,
    se_extrapolation_t* result
);

/**
 * @brief Detect when extrapolation is breaking down
 *
 * Compares predicted values against actual to detect model failure.
 *
 * @param se Software engineering handle
 * @param extrapolation Previous extrapolation
 * @param actual_value Actual observed value
 * @param actual_timestamp Timestamp of observation
 * @return true if extrapolation has failed
 */
bool software_eng_extrapolation_failed(
    software_eng_t* se,
    const se_extrapolation_t* extrapolation,
    float actual_value,
    uint64_t actual_timestamp
);

/**
 * @brief Free extrapolation result
 *
 * @param result Extrapolation to free
 */
void software_eng_free_extrapolation(se_extrapolation_t* result);

/**
 * @brief Get trend name
 *
 * @param trend Trend type
 * @return Human-readable name
 */
const char* software_eng_trend_to_string(se_trend_t trend);

/* ============================================================================
 * INSIGHT & SYNTHESIS API
 * ============================================================================ */

/**
 * @brief Generate insights from code analysis
 *
 * Synthesizes patterns, metrics, and hunches into actionable insights.
 *
 * @param se Software engineering handle
 * @param metrics Current metrics
 * @param patterns Detected patterns array
 * @param num_patterns Number of patterns
 * @param hunches Active hunches array
 * @param num_hunches Number of hunches
 * @param insights Output insights array
 * @param max_insights Maximum insights to generate
 * @return Number of insights generated, -1 on error
 */
int software_eng_generate_insights(
    software_eng_t* se,
    const software_metrics_t* metrics,
    const pattern_result_t* patterns,
    uint32_t num_patterns,
    const se_hunch_t* hunches,
    uint32_t num_hunches,
    se_insight_t* insights,
    uint32_t max_insights
);

/**
 * @brief Synthesize knowledge from multiple sources
 *
 * Combines metrics, patterns, smells, and extrapolations into unified understanding.
 *
 * @param se Software engineering handle
 * @param metrics Metrics array
 * @param num_metrics Number of metric snapshots
 * @param patterns Pattern results
 * @param num_patterns Number of patterns
 * @param smells Smell results
 * @param num_smells Number of smells
 * @param synthesis Output synthesis result
 * @return 0 on success
 */
int software_eng_synthesize(
    software_eng_t* se,
    const software_metrics_t* metrics,
    uint32_t num_metrics,
    const pattern_result_t* patterns,
    uint32_t num_patterns,
    const smell_result_t* smells,
    uint32_t num_smells,
    se_synthesis_t* synthesis
);

/**
 * @brief Free synthesis result
 *
 * @param synthesis Synthesis to free
 */
void software_eng_free_synthesis(se_synthesis_t* synthesis);

/**
 * @brief Identify knowledge gaps in current understanding
 *
 * @param se Software engineering handle
 * @param synthesis Current synthesis
 * @param gaps Output array of gap descriptions
 * @param max_gaps Maximum gaps to identify
 * @return Number of gaps identified
 */
int software_eng_identify_gaps(
    software_eng_t* se,
    const se_synthesis_t* synthesis,
    char** gaps,
    uint32_t max_gaps
);

/* ============================================================================
 * ANALOGICAL REASONING API
 * ============================================================================ */

/**
 * @brief Find analogy between current code and known patterns
 *
 * Uses structural mapping to find similar situations in known domains.
 *
 * @param se Software engineering handle
 * @param structure Current code structure
 * @param known_patterns Array of known pattern results
 * @param num_known Number of known patterns
 * @param analogy Output analogy
 * @return 0 on success, -1 if no analogy found
 */
int software_eng_find_analogy(
    software_eng_t* se,
    const code_structure_t* structure,
    const pattern_result_t* known_patterns,
    uint32_t num_known,
    se_analogy_t* analogy
);

/**
 * @brief Transfer knowledge using analogy
 *
 * Applies insights from analogous domain to current situation.
 *
 * @param se Software engineering handle
 * @param analogy The analogy to use
 * @param source_insight Insight from source domain
 * @param transferred Output transferred insight
 * @return 0 on success
 */
int software_eng_transfer_knowledge(
    software_eng_t* se,
    const se_analogy_t* analogy,
    const se_insight_t* source_insight,
    se_insight_t* transferred
);

/* ============================================================================
 * LEARNING & INTEGRATION API
 * ============================================================================ */

/**
 * @brief Learn from analysis outcome
 *
 * Updates internal models based on whether predictions/hunches were correct.
 *
 * @param se Software engineering handle
 * @param hunch Original hunch
 * @param was_correct Whether it was correct
 * @param feedback Additional feedback
 * @return 0 on success
 */
int software_eng_learn_from_outcome(
    software_eng_t* se,
    const se_hunch_t* hunch,
    bool was_correct,
    const char* feedback
);

/**
 * @brief Integrate new knowledge into intuition model
 *
 * @param se Software engineering handle
 * @param insight New insight to integrate
 * @return 0 on success
 */
int software_eng_integrate_knowledge(software_eng_t* se, const se_insight_t* insight);

/**
 * @brief Get intuition accuracy metrics
 *
 * @param se Software engineering handle
 * @param accuracy Output overall accuracy [0,1]
 * @param by_type Output accuracy by hunch type (optional, array of SE_HUNCH_UNKNOWN+1)
 * @return 0 on success
 */
int software_eng_get_intuition_accuracy(
    const software_eng_t* se,
    float* accuracy,
    float* by_type
);

/**
 * @brief Export learned intuition model
 *
 * @param se Software engineering handle
 * @param filename Output filename
 * @return 0 on success
 */
int software_eng_export_intuition(const software_eng_t* se, const char* filename);

/**
 * @brief Import learned intuition model
 *
 * @param se Software engineering handle
 * @param filename Input filename
 * @return 0 on success
 */
int software_eng_import_intuition(software_eng_t* se, const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOFTWARE_ENGINEERING_H */
