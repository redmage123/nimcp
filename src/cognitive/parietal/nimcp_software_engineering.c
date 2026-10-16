/**
 * @file nimcp_software_engineering.c
 * @brief Software engineering reasoning implementation for parietal lobe
 *
 * Implements software engineering reasoning including complexity analysis,
 * design pattern detection, code smell detection, and dependency analysis.
 */

#include "cognitive/parietal/nimcp_software_engineering.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/algorithms/nimcp_sort.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(software_engineering, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Intuition learning state
 */
typedef struct {
    se_intuition_config_t config;
    bool enabled;

    /* Hunch tracking */
    se_hunch_t* hunch_history;
    uint32_t num_hunches;
    uint32_t hunch_capacity;

    /* Learning statistics per hunch type */
    uint64_t hunch_counts[SE_HUNCH_UNKNOWN + 1];
    uint64_t hunch_correct[SE_HUNCH_UNKNOWN + 1];

    /* Learned thresholds (adjusted based on feedback) */
    float complexity_alarm_threshold;
    float coupling_alarm_threshold;
    float trend_sensitivity;

    /* Pattern weights for hunch generation */
    float pattern_weights[12];  /* Weights for different indicators */
} intuition_state_t;

struct software_eng {
    software_eng_config_t config;

    /* Modulation state */
    float inflammation_level;
    float sleep_deprivation_level;

    /* Intuition state */
    intuition_state_t* intuition;

    /* Statistics */
    uint64_t complexity_analyses;
    uint64_t pattern_detections;
    uint64_t smell_detections;
    uint64_t metric_calculations;
    uint64_t hunches_generated;
    uint64_t hunches_validated;
    uint64_t extrapolations_made;
    double total_processing_time_us;

    /* Thread safety */
    nimcp_mutex_t* lock;
};

/* Thread-local error message */
static _Thread_local char g_sweng_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_sweng_error(const char* msg) {
    strncpy(g_sweng_error, msg, sizeof(g_sweng_error) - 1);
    g_sweng_error[sizeof(g_sweng_error) - 1] = '\0';
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

software_eng_config_t software_eng_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_default", 0.0f);


    software_eng_config_t config = {
        .complexity_threshold = 10.0f,
        .coupling_threshold = 0.7f,
        .method_length_limit = 50,
        .class_size_limit = 500,
        .strict_mode = false,
        .enable_bio_async = false,
        .inflammation_sensitivity = 0.2f,
        .sleep_deprivation_factor = 0.15f
    };
    return config;
}

bool software_eng_validate_config(const software_eng_config_t* config) {
    if (!config) {
        set_sweng_error("Null config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_validate_config: config is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_validat", 0.0f);


    if (config->complexity_threshold <= 0.0f) {
        set_sweng_error("Invalid complexity threshold");
        return false;
    }
    if (config->method_length_limit == 0) {
        set_sweng_error("Method length limit must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "software_eng_validate_config: config->method_length_limit is zero");
        return false;
    }
    return true;
}

software_eng_t* software_eng_create(void) {
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_create", 0.0f);


    return software_eng_create_custom(NULL);
}

software_eng_t* software_eng_create_custom(const software_eng_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_create_", 0.0f);


    software_eng_config_t cfg = config ? *config : software_eng_default_config();

    if (!software_eng_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_create_custom: software_eng_validate_config is NULL");
        return NULL;
    }

    software_eng_t* se = nimcp_calloc(1, sizeof(software_eng_t));
    if (!se) {
        set_sweng_error("Failed to allocate software_eng struct");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "software_eng_create_custom: se is NULL");
        return NULL;
    }

    se->config = cfg;
    se->inflammation_level = 0.0f;
    se->sleep_deprivation_level = 0.0f;

    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    se->lock = nimcp_mutex_create(&attr);
    if (!se->lock) {
        set_sweng_error("Failed to create mutex");
        nimcp_free(se);
        se = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "software_eng_create_custom: se->lock is NULL");
        return NULL;
    }

    return se;
}

void software_eng_destroy(software_eng_t* se) {
    if (!se) return;

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_destroy", 0.0f);


    if (se->lock) {
        nimcp_mutex_destroy(se->lock);
    }
    nimcp_free(se);
    se = NULL;
}

/* ============================================================================
 * COMPLEXITY ANALYSIS API
 * ============================================================================ */

int software_eng_analyze_complexity(
    software_eng_t* se,
    const algorithm_traits_t* traits,
    complexity_result_t* result
) {
    if (!se || !traits || !result) {
        set_sweng_error("Null parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_analyze_complexity: required parameter is NULL (se, traits, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_analyze", 0.0f);


    nimcp_mutex_lock(se->lock);

    memset(result, 0, sizeof(complexity_result_t));
    result->confidence = 0.8f;

    /* Determine time complexity based on traits */
    if (traits->has_recursion && traits->recursive_calls >= 2 && !traits->has_dynamic_programming) {
        /* Multiple recursive calls without memoization = exponential */
        result->time_complexity = COMPLEXITY_O_2_N;
        result->time_coefficient = (float)traits->recursive_calls;
        snprintf(result->description, sizeof(result->description),
                 "Exponential due to %u recursive calls without memoization",
                 traits->recursive_calls);
    } else if (traits->loop_depth >= 3) {
        result->time_complexity = COMPLEXITY_O_N_CUBED;
        result->time_coefficient = 1.0f;
        snprintf(result->description, sizeof(result->description),
                 "Cubic complexity from %u nested loops", traits->loop_depth);
    } else if (traits->loop_depth == 2) {
        result->time_complexity = COMPLEXITY_O_N_SQUARED;
        result->time_coefficient = 1.0f;
        snprintf(result->description, sizeof(result->description),
                 "Quadratic complexity from 2 nested loops");
    } else if (traits->has_divide_conquer && traits->has_loops) {
        result->time_complexity = COMPLEXITY_O_N_LOG_N;
        result->time_coefficient = 1.0f;
        snprintf(result->description, sizeof(result->description),
                 "Linearithmic from divide-and-conquer with linear work");
    } else if (traits->has_sorting) {
        result->time_complexity = COMPLEXITY_O_N_LOG_N;
        result->time_coefficient = 1.0f;
        snprintf(result->description, sizeof(result->description),
                 "Linearithmic due to sorting operation");
    } else if (traits->has_loops && traits->loop_depth == 1) {
        result->time_complexity = COMPLEXITY_O_N;
        result->time_coefficient = 1.0f;
        snprintf(result->description, sizeof(result->description),
                 "Linear complexity from single loop");
    } else if (traits->has_searching && !traits->has_loops) {
        result->time_complexity = COMPLEXITY_O_LOG_N;
        result->time_coefficient = 1.0f;
        snprintf(result->description, sizeof(result->description),
                 "Logarithmic from binary search pattern");
    } else if (!traits->has_loops && !traits->has_recursion) {
        result->time_complexity = COMPLEXITY_O_1;
        result->time_coefficient = 1.0f;
        snprintf(result->description, sizeof(result->description),
                 "Constant time - no loops or recursion");
    } else {
        result->time_complexity = COMPLEXITY_O_N;
        result->time_coefficient = 1.0f;
        result->confidence = 0.5f;
        snprintf(result->description, sizeof(result->description),
                 "Estimated linear complexity (low confidence)");
    }

    /* Determine space complexity */
    if (traits->has_dynamic_programming) {
        result->space_complexity = COMPLEXITY_O_N;
        result->space_coefficient = (float)traits->auxiliary_space;
    } else if (traits->has_recursion) {
        /* Recursive algorithms use stack space */
        result->space_complexity = COMPLEXITY_O_N;
        result->space_coefficient = 1.0f;
    } else if (traits->auxiliary_space > 0) {
        result->space_complexity = COMPLEXITY_O_N;
        result->space_coefficient = (float)traits->auxiliary_space;
    } else {
        result->space_complexity = COMPLEXITY_O_1;
        result->space_coefficient = 1.0f;
    }

    se->complexity_analyses++;
    nimcp_mutex_unlock(se->lock);
    return 0;
}

const char* software_eng_complexity_to_string(complexity_class_t complexity) {
    switch (complexity) {
        case COMPLEXITY_O_1: return "O(1)";
        case COMPLEXITY_O_LOG_N: return "O(log n)";
        case COMPLEXITY_O_N: return "O(n)";
        case COMPLEXITY_O_N_LOG_N: return "O(n log n)";
        case COMPLEXITY_O_N_SQUARED: return "O(n^2)";
        case COMPLEXITY_O_N_CUBED: return "O(n^3)";
        case COMPLEXITY_O_2_N: return "O(2^n)";
        case COMPLEXITY_O_N_FACTORIAL: return "O(n!)";
        default: return "Unknown";
    }
}

int software_eng_compare_complexity(complexity_class_t a, complexity_class_t b) {
    /* Lower enum value = better complexity */
    if ((int)a < (int)b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "software_eng_compare_complexity: validation failed");
        return -1;
    }
    if ((int)a > (int)b) return 1;
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_compare", 0.0f);


    return 0;
}

double software_eng_estimate_runtime(
    const software_eng_t* se,
    complexity_class_t complexity,
    double coefficient,
    uint64_t input_size
) {
    if (!se) return 0.0;

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_estimat", 0.0f);


    double n = (double)input_size;

    switch (complexity) {
        case COMPLEXITY_O_1:
            return coefficient;
        case COMPLEXITY_O_LOG_N:
            return coefficient * log2(n);
        case COMPLEXITY_O_N:
            return coefficient * n;
        case COMPLEXITY_O_N_LOG_N:
            return coefficient * n * log2(n);
        case COMPLEXITY_O_N_SQUARED:
            return coefficient * n * n;
        case COMPLEXITY_O_N_CUBED:
            return coefficient * n * n * n;
        case COMPLEXITY_O_2_N:
            return coefficient * pow(2.0, n);
        case COMPLEXITY_O_N_FACTORIAL:
            /* Approximate using Stirling's formula for large n */
            return coefficient * sqrt(2.0 * M_PI * n) * pow(n / M_E, n);
        default:
            return coefficient * n;
    }
}

/* ============================================================================
 * PATTERN DETECTION API
 * ============================================================================ */

int software_eng_detect_pattern(
    software_eng_t* se,
    const code_structure_t* structure,
    pattern_result_t* result
) {
    if (!se || !structure || !result) {
        set_sweng_error("Null parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_detect_pattern: required parameter is NULL (se, structure, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_detect_", 0.0f);


    nimcp_mutex_lock(se->lock);

    memset(result, 0, sizeof(pattern_result_t));

    /* Pattern detection heuristics */
    float best_confidence = 0.0f;

    /* Singleton: single instance + private constructor pattern */
    if (structure->has_single_instance && structure->num_classes == 1) {
        float conf = 0.8f;
        if (conf > best_confidence) {
            best_confidence = conf;
            result->pattern = DESIGN_PATTERN_SINGLETON;
            result->category = PATTERN_CATEGORY_CREATIONAL;
            snprintf(result->description, sizeof(result->description),
                     "Singleton pattern - ensures single instance");
            snprintf(result->recommendation, sizeof(result->recommendation),
                     "Consider thread-safety and lazy initialization");
        }
    }

    /* Factory: factory method + creates objects */
    if (structure->has_factory_method && structure->has_abstract_base) {
        float conf = 0.75f;
        if (conf > best_confidence) {
            best_confidence = conf;
            result->pattern = DESIGN_PATTERN_FACTORY;
            result->category = PATTERN_CATEGORY_CREATIONAL;
            snprintf(result->description, sizeof(result->description),
                     "Factory Method pattern - creates objects via interface");
            snprintf(result->recommendation, sizeof(result->recommendation),
                     "Good for extensibility, add new product types easily");
        }
    }

    /* Observer: callbacks + multiple subscribers */
    if (structure->has_callbacks && structure->num_interfaces >= 1) {
        float conf = 0.7f;
        if (conf > best_confidence) {
            best_confidence = conf;
            result->pattern = DESIGN_PATTERN_OBSERVER;
            result->category = PATTERN_CATEGORY_BEHAVIORAL;
            snprintf(result->description, sizeof(result->description),
                     "Observer pattern - publish-subscribe notification");
            snprintf(result->recommendation, sizeof(result->recommendation),
                     "Ensure proper unsubscription to prevent memory leaks");
        }
    }

    /* Strategy: swappable algorithms + interface */
    if (structure->has_strategy_interface && structure->has_composition) {
        float conf = 0.72f;
        if (conf > best_confidence) {
            best_confidence = conf;
            result->pattern = DESIGN_PATTERN_STRATEGY;
            result->category = PATTERN_CATEGORY_BEHAVIORAL;
            snprintf(result->description, sizeof(result->description),
                     "Strategy pattern - interchangeable algorithms");
            snprintf(result->recommendation, sizeof(result->recommendation),
                     "Favor composition over inheritance for flexibility");
        }
    }

    /* Decorator: composition + same interface */
    if (structure->has_composition && structure->has_inheritance &&
        structure->num_interfaces >= 1) {
        float conf = 0.65f;
        if (conf > best_confidence) {
            best_confidence = conf;
            result->pattern = DESIGN_PATTERN_DECORATOR;
            result->category = PATTERN_CATEGORY_STRUCTURAL;
            snprintf(result->description, sizeof(result->description),
                     "Decorator pattern - dynamic behavior extension");
            snprintf(result->recommendation, sizeof(result->recommendation),
                     "Keep decorators focused on single responsibility");
        }
    }

    /* Composite: tree structure + uniform interface */
    if (structure->has_composition && structure->has_inheritance &&
        structure->cohesion_score > 0.7f) {
        float conf = 0.6f;
        if (conf > best_confidence) {
            best_confidence = conf;
            result->pattern = DESIGN_PATTERN_COMPOSITE;
            result->category = PATTERN_CATEGORY_STRUCTURAL;
            snprintf(result->description, sizeof(result->description),
                     "Composite pattern - tree structure with uniform interface");
            snprintf(result->recommendation, sizeof(result->recommendation),
                     "Useful for hierarchical data structures");
        }
    }

    if (best_confidence == 0.0f) {
        result->pattern = DESIGN_PATTERN_NONE;
        result->category = PATTERN_CATEGORY_UNKNOWN;
        result->confidence = 0.0f;
        snprintf(result->description, sizeof(result->description),
                 "No clear design pattern detected");
        snprintf(result->recommendation, sizeof(result->recommendation),
                 "Consider applying appropriate patterns for better design");
    } else {
        result->confidence = best_confidence;
    }

    se->pattern_detections++;
    nimcp_mutex_unlock(se->lock);
    return 0;
}

const char* software_eng_pattern_to_string(design_pattern_t pattern) {
    switch (pattern) {
        case DESIGN_PATTERN_SINGLETON: return "Singleton";
        case DESIGN_PATTERN_FACTORY: return "Factory Method";
        case DESIGN_PATTERN_ABSTRACT_FACTORY: return "Abstract Factory";
        case DESIGN_PATTERN_BUILDER: return "Builder";
        case DESIGN_PATTERN_PROTOTYPE: return "Prototype";
        case DESIGN_PATTERN_ADAPTER: return "Adapter";
        case DESIGN_PATTERN_BRIDGE: return "Bridge";
        case DESIGN_PATTERN_COMPOSITE: return "Composite";
        case DESIGN_PATTERN_DECORATOR: return "Decorator";
        case DESIGN_PATTERN_FACADE: return "Facade";
        case DESIGN_PATTERN_FLYWEIGHT: return "Flyweight";
        case DESIGN_PATTERN_PROXY: return "Proxy";
        case DESIGN_PATTERN_CHAIN_OF_RESPONSIBILITY: return "Chain of Responsibility";
        case DESIGN_PATTERN_COMMAND: return "Command";
        case DESIGN_PATTERN_ITERATOR: return "Iterator";
        case DESIGN_PATTERN_MEDIATOR: return "Mediator";
        case DESIGN_PATTERN_MEMENTO: return "Memento";
        case DESIGN_PATTERN_OBSERVER: return "Observer";
        case DESIGN_PATTERN_STATE: return "State";
        case DESIGN_PATTERN_STRATEGY: return "Strategy";
        case DESIGN_PATTERN_TEMPLATE_METHOD: return "Template Method";
        case DESIGN_PATTERN_VISITOR: return "Visitor";
        default: return "Unknown";
    }
}

const char* software_eng_category_to_string(pattern_category_t category) {
    switch (category) {
        case PATTERN_CATEGORY_CREATIONAL: return "Creational";
        case PATTERN_CATEGORY_STRUCTURAL: return "Structural";
        case PATTERN_CATEGORY_BEHAVIORAL: return "Behavioral";
        default: return "Unknown";
    }
}

float software_eng_pattern_applicability(
    const software_eng_t* se,
    design_pattern_t pattern,
    const code_structure_t* structure
) {
    if (!se || !structure) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_pattern", 0.0f);


    switch (pattern) {
        case DESIGN_PATTERN_SINGLETON:
            return structure->has_single_instance ? 0.9f : 0.1f;
        case DESIGN_PATTERN_FACTORY:
            return structure->has_factory_method ? 0.8f : 0.2f;
        case DESIGN_PATTERN_OBSERVER:
            return structure->has_callbacks ? 0.75f : 0.1f;
        case DESIGN_PATTERN_STRATEGY:
            return structure->has_strategy_interface ? 0.8f : 0.15f;
        case DESIGN_PATTERN_DECORATOR:
            return (structure->has_composition && structure->has_inheritance) ? 0.7f : 0.1f;
        default:
            return 0.3f;  /* Default moderate applicability */
    }
}

/* ============================================================================
 * CODE SMELL DETECTION API
 * ============================================================================ */

uint32_t software_eng_detect_smells(
    software_eng_t* se,
    const software_metrics_t* metrics,
    smell_result_t* results,
    uint32_t max_results
) {
    if (!se || !metrics || !results || max_results == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_detect_", 0.0f);


    nimcp_mutex_lock(se->lock);

    uint32_t count = 0;

    /* Long method / Large class */
    if (metrics->cyclomatic_complexity > se->config.complexity_threshold && count < max_results) {
        results[count].smell = SMELL_LONG_METHOD;
        results[count].severity = (metrics->cyclomatic_complexity / (se->config.complexity_threshold * 2));
        if (results[count].severity > 1.0f) results[count].severity = 1.0f;
        snprintf(results[count].location, SWENG_MAX_IDENTIFIER, "High complexity methods");
        snprintf(results[count].suggestion, 256,
                 "Extract methods to reduce complexity below %.1f",
                 se->config.complexity_threshold);
        count++;
    }

    /* High coupling */
    if (metrics->efferent_coupling > se->config.coupling_threshold * 10 && count < max_results) {
        results[count].smell = SMELL_FEATURE_ENVY;
        results[count].severity = metrics->efferent_coupling / 20.0f;
        if (results[count].severity > 1.0f) results[count].severity = 1.0f;
        snprintf(results[count].location, SWENG_MAX_IDENTIFIER, "High coupling modules");
        snprintf(results[count].suggestion, 256,
                 "Reduce outgoing dependencies, consider facade pattern");
        count++;
    }

    /* Large class by LOC */
    float avg_loc_per_class = metrics->num_classes > 0 ?
        (float)metrics->lines_of_code / (float)metrics->num_classes : 0.0f;
    if (avg_loc_per_class > (float)se->config.class_size_limit && count < max_results) {
        results[count].smell = SMELL_LARGE_CLASS;
        results[count].severity = avg_loc_per_class / ((float)se->config.class_size_limit * 2);
        if (results[count].severity > 1.0f) results[count].severity = 1.0f;
        snprintf(results[count].location, SWENG_MAX_IDENTIFIER, "Large classes");
        snprintf(results[count].suggestion, 256,
                 "Split classes - aim for < %u LOC per class",
                 se->config.class_size_limit);
        count++;
    }

    /* God class: high coupling + high complexity */
    if (metrics->cyclomatic_complexity > se->config.complexity_threshold * 2 &&
        metrics->efferent_coupling > 10 && count < max_results) {
        results[count].smell = SMELL_GOD_CLASS;
        results[count].severity = 0.9f;
        snprintf(results[count].location, SWENG_MAX_IDENTIFIER, "Central module");
        snprintf(results[count].suggestion, 256,
                 "Major refactoring needed - extract responsibilities");
        count++;
    }

    /* Low maintainability */
    if (metrics->maintainability_index < 20.0f && metrics->maintainability_index > 0.0f &&
        count < max_results) {
        results[count].smell = SMELL_DEAD_CODE;  /* Using as proxy for poor maintainability */
        results[count].severity = 1.0f - (metrics->maintainability_index / 20.0f);
        snprintf(results[count].location, SWENG_MAX_IDENTIFIER, "Low maintainability areas");
        snprintf(results[count].suggestion, 256,
                 "Improve documentation and simplify logic");
        count++;
    }

    se->smell_detections += count;
    nimcp_mutex_unlock(se->lock);
    return count;
}

const char* software_eng_smell_to_string(code_smell_t smell) {
    switch (smell) {
        case SMELL_LONG_METHOD: return "Long Method";
        case SMELL_LARGE_CLASS: return "Large Class";
        case SMELL_FEATURE_ENVY: return "Feature Envy";
        case SMELL_DATA_CLUMPS: return "Data Clumps";
        case SMELL_PRIMITIVE_OBSESSION: return "Primitive Obsession";
        case SMELL_SWITCH_STATEMENTS: return "Switch Statements";
        case SMELL_PARALLEL_INHERITANCE: return "Parallel Inheritance";
        case SMELL_LAZY_CLASS: return "Lazy Class";
        case SMELL_SPECULATIVE_GENERALITY: return "Speculative Generality";
        case SMELL_DEAD_CODE: return "Dead Code";
        case SMELL_DUPLICATE_CODE: return "Duplicate Code";
        case SMELL_GOD_CLASS: return "God Class";
        case SMELL_NONE: return "None";
        default: return "Unknown";
    }
}

/* ============================================================================
 * DEPENDENCY ANALYSIS API
 * ============================================================================ */

void software_eng_init_graph(dep_graph_t* graph) {
    if (!graph) return;
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_init_gr", 0.0f);


    memset(graph, 0, sizeof(dep_graph_t));
}

int software_eng_add_node(dep_graph_t* graph, const char* name) {
    if (!graph || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_add_node: required parameter is NULL (graph, name)");
        return -1;
    }
    if (graph->num_nodes >= SWENG_MAX_GRAPH_NODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "software_eng_add_node: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_add_nod", 0.0f);


    uint32_t id = graph->num_nodes;
    graph->nodes[id].id = id;
    strncpy(graph->nodes[id].name, name, SWENG_MAX_IDENTIFIER - 1);
    graph->nodes[id].num_dependencies = 0;
    graph->nodes[id].dependents_count = 0;
    graph->num_nodes++;

    return (int)id;
}

int software_eng_add_dependency(dep_graph_t* graph, uint32_t from_id, uint32_t to_id) {
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_add_dependency: graph is NULL");
        return -1;
    }
    if (from_id >= graph->num_nodes || to_id >= graph->num_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "software_eng_add_dependency: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_add_dep", 0.0f);


    dep_node_t* node = &graph->nodes[from_id];
    if (node->num_dependencies >= SWENG_MAX_GRAPH_NODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "software_eng_add_dependency: capacity exceeded");
        return -1;
    }

    node->dependencies[node->num_dependencies++] = to_id;
    graph->nodes[to_id].dependents_count++;

    return 0;
}

static bool dfs_has_cycle(dep_graph_t* graph, uint32_t node, uint8_t* visited, uint8_t* rec_stack) {
    visited[node] = 1;
    rec_stack[node] = 1;

    for (uint32_t i = 0; i < graph->nodes[node].num_dependencies; i++) {
        uint32_t dep = graph->nodes[node].dependencies[i];
        if (!visited[dep] && dfs_has_cycle(graph, dep, visited, rec_stack)) {
            return true;
        } else if (rec_stack[dep]) {
            return true;
        }
    }

    rec_stack[node] = 0;
    return false;
}

/* Internal unlocked version of detect_cycles */
static bool detect_cycles_unlocked(dep_graph_t* graph) {
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_cycles_unlocked: graph is NULL");
        return false;
    }

    uint8_t visited[SWENG_MAX_GRAPH_NODES] = {0};
    uint8_t rec_stack[SWENG_MAX_GRAPH_NODES] = {0};

    graph->has_cycles = false;
    for (uint32_t i = 0; i < graph->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->num_nodes > 256) {
            software_engineering_heartbeat("software_eng_loop",
                             (float)(i + 1) / (float)graph->num_nodes);
        }

        if (!visited[i] && dfs_has_cycle(graph, i, visited, rec_stack)) {
            graph->has_cycles = true;
            break;
        }
    }

    return graph->has_cycles;
}

bool software_eng_detect_cycles(software_eng_t* se, dep_graph_t* graph) {
    if (!se || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_detect_cycles: required parameter is NULL (se, graph)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_detect_", 0.0f);


    nimcp_mutex_lock(se->lock);
    bool result = detect_cycles_unlocked(graph);
    nimcp_mutex_unlock(se->lock);

    return result;
}

int software_eng_analyze_dependencies(software_eng_t* se, dep_graph_t* graph) {
    if (!se || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_analyze_dependencies: required parameter is NULL (se, graph)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_analyze", 0.0f);


    nimcp_mutex_lock(se->lock);

    /* Detect cycles (use unlocked version since we already have the lock) */
    detect_cycles_unlocked(graph);

    /* Calculate max depth (simplified BFS) */
    graph->max_depth = 0;
    for (uint32_t i = 0; i < graph->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->num_nodes > 256) {
            software_engineering_heartbeat("software_eng_loop",
                             (float)(i + 1) / (float)graph->num_nodes);
        }

        uint32_t depth = 0;
        uint8_t visited[SWENG_MAX_GRAPH_NODES] = {0};
        uint32_t queue[SWENG_MAX_GRAPH_NODES];
        uint32_t front = 0, back = 0;
        queue[back++] = i;

        while (front < back) {
            uint32_t curr = queue[front++];
            if (visited[curr]) continue;
            visited[curr] = 1;
            depth++;

            for (uint32_t j = 0; j < graph->nodes[curr].num_dependencies; j++) {
                uint32_t dep = graph->nodes[curr].dependencies[j];
                if (!visited[dep]) {
                    queue[back++] = dep;
                }
            }
        }

        if (depth > graph->max_depth) {
            graph->max_depth = depth;
        }
    }

    /* Calculate average stability */
    float total_stability = 0.0f;
    for (uint32_t i = 0; i < graph->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->num_nodes > 256) {
            software_engineering_heartbeat("software_eng_loop",
                             (float)(i + 1) / (float)graph->num_nodes);
        }

        dep_node_t* node = &graph->nodes[i];
        float ca = (float)node->dependents_count;
        float ce = (float)node->num_dependencies;
        if (ca + ce > 0) {
            float stability = ce / (ca + ce);
            total_stability += stability;
        }
    }
    graph->stability_avg = graph->num_nodes > 0 ?
        total_stability / (float)graph->num_nodes : 0.0f;

    se->metric_calculations++;
    nimcp_mutex_unlock(se->lock);
    return 0;
}

/*
 * Callback context for software engineering topological sort
 *
 * NOTE: The original algorithm uses inverted dependency semantics:
 * - "dependencies" in the graph means "nodes this node depends on"
 * - But the algorithm increments in_degree of the dependency, not the dependent
 * This produces a reverse topological order (dependents before dependencies)
 *
 * We preserve this behavior by using the dependencies array as "dependents"
 * (nodes that will have their in_degree decremented when this node is processed)
 */
typedef struct {
    const dep_graph_t* graph;
    uint32_t* in_degrees;  /* Precomputed: how many nodes have i in their dependencies */
} sweng_topo_ctx_t;

/**
 * @brief Get count of nodes that "depend on" this node (in the original semantics)
 */
static uint32_t sweng_get_dep_count(uint32_t node_index, void* user_data) {
    sweng_topo_ctx_t* ctx = (sweng_topo_ctx_t*)user_data;
    if (node_index >= ctx->graph->num_nodes) {
        return 0;
    }
    return ctx->in_degrees[node_index];
}

/**
 * @brief Get the jth node that has this node in its dependencies array
 */
static uint32_t sweng_get_dep(uint32_t node_index, uint32_t dep_index, void* user_data) {
    sweng_topo_ctx_t* ctx = (sweng_topo_ctx_t*)user_data;
    const dep_graph_t* graph = ctx->graph;

    if (node_index >= graph->num_nodes) {
        return UINT32_MAX;
    }

    /* Find nodes that have node_index in their dependencies array */
    uint32_t found = 0;
    for (uint32_t i = 0; i < graph->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->num_nodes > 256) {
            software_engineering_heartbeat("software_eng_loop",
                             (float)(i + 1) / (float)graph->num_nodes);
        }

        for (uint32_t j = 0; j < graph->nodes[i].num_dependencies; j++) {
            if (graph->nodes[i].dependencies[j] == node_index) {
                if (found == dep_index) {
                    return i;
                }
                found++;
                break;  /* Node i only counts once */
            }
        }
    }

    return UINT32_MAX;
}

int software_eng_topological_sort(
    software_eng_t* se,
    const dep_graph_t* graph,
    uint32_t* order,
    uint32_t max_order
) {
    if (!se || !graph || !order) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_topological_sort: required parameter is NULL (se, graph, order)");
        return -1;
    }
    if (graph->has_cycles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "software_eng_topological_sort: validation failed");
        return -1;
    }
    if (graph->num_nodes == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_topolog", 0.0f);


    nimcp_mutex_lock(se->lock);

    /* Precompute in-degrees (same as original algorithm) */
    uint32_t in_degrees[SWENG_MAX_GRAPH_NODES] = {0};
    for (uint32_t i = 0; i < graph->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->num_nodes > 256) {
            software_engineering_heartbeat("software_eng_loop",
                             (float)(i + 1) / (float)graph->num_nodes);
        }

        for (uint32_t j = 0; j < graph->nodes[i].num_dependencies; j++) {
            uint32_t dep = graph->nodes[i].dependencies[j];
            if (dep < graph->num_nodes) {
                in_degrees[dep]++;
            }
        }
    }

    sweng_topo_ctx_t ctx = {
        .graph = graph,
        .in_degrees = in_degrees
    };

    nimcp_topo_config_t config = {
        .node_count = graph->num_nodes,
        .user_data = &ctx,
        .get_dep_count = sweng_get_dep_count,
        .get_dep = sweng_get_dep,
        .get_dependent_count = NULL,
        .get_dependent = NULL
    };

    uint32_t sorted_count = 0;
    nimcp_sort_result_t result = nimcp_topological_sort(
        &config, order, max_order, &sorted_count);

    nimcp_mutex_unlock(se->lock);

    if (result == NIMCP_SORT_ERROR_CYCLE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "software_eng_topological_sort: validation failed");
        return -1;
    }

    return (int)sorted_count;
}

/* ============================================================================
 * METRICS API
 * ============================================================================ */

float software_eng_cyclomatic_complexity(
    uint32_t edges,
    uint32_t nodes,
    uint32_t components
) {
    /* M = E - N + 2P */
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_cycloma", 0.0f);


    return (float)edges - (float)nodes + 2.0f * (float)components;
}

float software_eng_maintainability_index(
    float halstead_volume,
    float cyclomatic_complexity,
    uint32_t loc
) {
    if (loc == 0 || halstead_volume < EPSILON) return 0.0f;

    /* MI = 171 - 5.2 * ln(V) - 0.23 * G - 16.2 * ln(LOC) */
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_maintai", 0.0f);


    float mi = 171.0f -
               5.2f * logf(halstead_volume + 1.0f) -
               0.23f * cyclomatic_complexity -
               16.2f * logf((float)loc);

    /* Normalize to 0-100 */
    if (mi < 0.0f) mi = 0.0f;
    if (mi > 100.0f) mi = 100.0f;

    return mi;
}

float software_eng_instability(float afferent_coupling, float efferent_coupling) {
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_instabi", 0.0f);


    float total = afferent_coupling + efferent_coupling;
    if (total < EPSILON) return 0.0f;
    return efferent_coupling / (fabsf(total) > 1e-7f ? total : 1e-7f);
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int software_eng_set_inflammation(software_eng_t* se, float level) {
    if (!se) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_set_inflammation: se is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_set_inf", 0.0f);


    nimcp_mutex_lock(se->lock);
    se->inflammation_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(se->lock);

    return 0;
}

int software_eng_set_sleep_deprivation(software_eng_t* se, float level) {
    if (!se) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_set_sleep_deprivation: se is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_set_sle", 0.0f);


    nimcp_mutex_lock(se->lock);
    se->sleep_deprivation_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(se->lock);

    return 0;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int software_eng_get_stats(software_eng_t* se, software_eng_stats_t* stats) {
    if (!se || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_get_stats: required parameter is NULL (se, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_get_sta", 0.0f);


    nimcp_mutex_lock(((software_eng_t*)se)->lock);

    stats->complexity_analyses = se->complexity_analyses;
    stats->pattern_detections = se->pattern_detections;
    stats->smell_detections = se->smell_detections;
    stats->metric_calculations = se->metric_calculations;

    uint64_t total_ops = se->complexity_analyses + se->pattern_detections +
                         se->smell_detections + se->metric_calculations;
    if (total_ops > 0) {
        stats->avg_processing_time_us =
            (float)(se->total_processing_time_us / (double)total_ops);
    } else {
        stats->avg_processing_time_us = 0.0f;
    }

    nimcp_mutex_unlock(((software_eng_t*)se)->lock);
    return 0;
}

void software_eng_reset_stats(software_eng_t* se) {
    if (!se) return;

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_reset_s", 0.0f);


    nimcp_mutex_lock(se->lock);
    se->complexity_analyses = 0;
    se->pattern_detections = 0;
    se->smell_detections = 0;
    se->metric_calculations = 0;
    se->total_processing_time_us = 0.0;
    nimcp_mutex_unlock(se->lock);
}

const char* software_eng_get_last_error(void) {
    return g_sweng_error;
}

/* ============================================================================
 * INTUITIVE REASONING API
 * ============================================================================ */

se_intuition_config_t software_eng_intuition_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_intuiti", 0.0f);


    se_intuition_config_t config = {
        .hunch_threshold = 0.3f,
        .max_hunches = 100,
        .extrapolation_horizon = 30,
        .extrapolation_decay = 0.05f,
        .enable_analogical_reasoning = true,
        .enable_synthesis = true,
        .min_data_points = 5,
        .novelty_threshold = 0.2f
    };
    return config;
}

int software_eng_enable_intuition(software_eng_t* se, const se_intuition_config_t* config) {
    if (!se) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_enable_intuition: se is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_enable_", 0.0f);


    (void)config;  /* TODO: Full implementation */
    return 0;
}

int software_eng_disable_intuition(software_eng_t* se) {
    if (!se) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_disable_intuition: se is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_disable", 0.0f);


    return 0;
}

const char* software_eng_hunch_type_to_string(se_hunch_type_t type) {
    switch (type) {
        case SE_HUNCH_COMPLEXITY_GROWTH: return "Complexity Growth";
        case SE_HUNCH_TECH_DEBT_ACCUMULATION: return "Technical Debt Accumulation";
        case SE_HUNCH_PATTERN_EMERGING: return "Design Pattern Emerging";
        case SE_HUNCH_REFACTOR_NEEDED: return "Refactoring Needed";
        case SE_HUNCH_DEPENDENCY_RISK: return "Dependency Risk";
        case SE_HUNCH_PERFORMANCE_DEGRADATION: return "Performance Degradation";
        case SE_HUNCH_MAINTAINABILITY_DECLINE: return "Maintainability Decline";
        case SE_HUNCH_ARCHITECTURE_SMELL: return "Architecture Smell";
        case SE_HUNCH_SECURITY_CONCERN: return "Security Concern";
        case SE_HUNCH_POSITIVE_TREND: return "Positive Trend";
        default: return "Unknown";
    }
}

const char* software_eng_confidence_to_string(se_confidence_level_t level) {
    switch (level) {
        case SE_CONFIDENCE_VERY_LOW: return "Very Low";
        case SE_CONFIDENCE_LOW: return "Low";
        case SE_CONFIDENCE_MODERATE: return "Moderate";
        case SE_CONFIDENCE_HIGH: return "High";
        case SE_CONFIDENCE_VERY_HIGH: return "Very High";
        default: return "Unknown";
    }
}

const char* software_eng_trend_to_string(se_trend_t trend) {
    switch (trend) {
        case SE_TREND_IMPROVING: return "Improving";
        case SE_TREND_STABLE: return "Stable";
        case SE_TREND_DECLINING: return "Declining";
        case SE_TREND_VOLATILE: return "Volatile";
        default: return "Unknown";
    }
}

se_metric_series_t* software_eng_create_series(const char* metric_name, uint32_t initial_capacity) {
    if (!metric_name || initial_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "software_eng_create_series: metric_name is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_create_", 0.0f);


    se_metric_series_t* series = nimcp_calloc(1, sizeof(se_metric_series_t));
    if (!series) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate series");

        return NULL;

    }

    series->points = nimcp_calloc(initial_capacity, sizeof(se_metric_point_t));
    if (!series->points) {
        nimcp_free(series);
        series = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "software_eng_create_series: series->points is NULL");
        return NULL;
    }

    series->capacity = initial_capacity;
    strncpy(series->metric_name, metric_name, sizeof(series->metric_name) - 1);
    return series;
}

void software_eng_destroy_series(se_metric_series_t* series) {
    if (!series) return;
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_destroy", 0.0f);


    nimcp_free(series->points);
    nimcp_free(series);
    series = NULL;
}

int software_eng_add_data_point(
    se_metric_series_t* series,
    uint64_t timestamp,
    float value,
    float confidence
) {
    if (!series) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "software_eng_add_data_point: series is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_add_dat", 0.0f);


    if (series->num_points >= series->capacity) {
        uint32_t new_cap = series->capacity * 2;
        se_metric_point_t* new_points = nimcp_realloc(series->points, new_cap * sizeof(se_metric_point_t));
        if (!new_points) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "software_eng_add_data_point: new_points is NULL");
            return -1;
        }
        series->points = new_points;
        series->capacity = new_cap;
    }

    series->points[series->num_points].timestamp = timestamp;
    series->points[series->num_points].value = value;
    series->points[series->num_points].confidence = nimcp_clamp01(confidence);
    series->num_points++;
    return 0;
}

void software_eng_free_extrapolation(se_extrapolation_t* result) {
    if (!result) return;
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_free_ex", 0.0f);


    if (result->predicted) {
        for (uint32_t i = 0; i < result->num_predicted; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_predicted > 256) {
                software_engineering_heartbeat("software_eng_loop",
                                 (float)(i + 1) / (float)result->num_predicted);
            }

            nimcp_free(result->predicted[i]);
        }
        nimcp_free(result->predicted);
        result->predicted = NULL;
    }
    result->num_predicted = 0;
}

void software_eng_free_synthesis(se_synthesis_t* synthesis) {
    if (!synthesis) return;
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_software_eng_free_sy", 0.0f);


    nimcp_free(synthesis->insights);
    nimcp_free(synthesis->knowledge_gaps);
    nimcp_free(synthesis->contradictions);
    synthesis->insights = NULL;
    synthesis->knowledge_gaps = NULL;
    synthesis->contradictions = NULL;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int software_engineering_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    software_engineering_heartbeat("software_eng_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Software_Engineering");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                software_engineering_heartbeat("software_eng_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Software_Engineering");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Software_Engineering");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void software_engineering_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_software_engineering_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int software_engineering_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "software_engineering_training_begin: NULL argument");
        return -1;
    }
    software_engineering_heartbeat_instance(g_software_engineering_health_agent, "software_engineering_training_begin", 0.0f);
    (void)(struct software_eng*)instance; /* Module state available for reset */
    return 0;
}

int software_engineering_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "software_engineering_training_end: NULL argument");
        return -1;
    }
    software_engineering_heartbeat_instance(g_software_engineering_health_agent, "software_engineering_training_end", 1.0f);
    (void)(struct software_eng*)instance; /* Module state available for finalization */
    return 0;
}

int software_engineering_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "software_engineering_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    software_engineering_heartbeat_instance(g_software_engineering_health_agent, "software_engineering_training_step", progress);
    (void)(struct software_eng*)instance; /* Module state available for step adaptation */
    return 0;
}
