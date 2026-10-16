//=============================================================================
// nimcp_language_logic_bridge.h - Language Layer Logic/Reasoning Integration
//=============================================================================
/**
 * @file nimcp_language_logic_bridge.h
 * @brief Bridge between Language Layer and Symbolic Logic Module
 *
 * WHAT: Integrates logical reasoning with language comprehension/production
 * WHY:  Language understanding requires inference, entailment, consistency
 * HOW:  Converts linguistic structures to logic, reasons over them
 *
 * BIOLOGICAL BASIS:
 * - Left prefrontal cortex: Logical reasoning about language
 * - Angular gyrus: Semantic-logic integration
 * - Inferior frontal gyrus: Syntactic reasoning
 * - Hippocampus: Episodic reasoning and context
 *
 * LANGUAGE-LOGIC INTEGRATION:
 * - Semantic entailment: "All dogs are mammals" → dog(x) → mammal(x)
 * - Presupposition checking: Verify assumptions in utterances
 * - Implicature reasoning: What's implied but not stated
 * - Consistency checking: Detect contradictions in discourse
 * - Reference resolution: Logical binding of pronouns/anaphora
 *
 * APPLICATIONS:
 * - Question answering: Logical inference to derive answers
 * - Discourse coherence: Verify logical flow of conversation
 * - Pragmatic reasoning: Understand speaker intent
 * - Argument analysis: Evaluate logical validity
 *
 * @version 1.0.0 - Phase L8: Logic Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_LOGIC_BRIDGE_H
#define NIMCP_LANGUAGE_LOGIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_orchestrator language_orchestrator_t;
typedef struct symbolic_logic symbolic_logic_t;

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Logical operation type
 */
typedef enum {
    LANG_LOGIC_ENTAILMENT = 0,         /**< A entails B */
    LANG_LOGIC_CONTRADICTION,           /**< A contradicts B */
    LANG_LOGIC_CONSISTENCY,             /**< Check consistency */
    LANG_LOGIC_IMPLICATION,             /**< A implies B */
    LANG_LOGIC_PRESUPPOSITION,          /**< Check presupposition */
    LANG_LOGIC_REFERENCE,               /**< Resolve reference */
    LANG_LOGIC_INFERENCE,               /**< General inference */
    LANG_LOGIC_COUNT
} language_logic_operation_t;

/**
 * @brief Logical result type
 */
typedef enum {
    LANG_LOGIC_RESULT_TRUE = 0,        /**< Logically true */
    LANG_LOGIC_RESULT_FALSE,           /**< Logically false */
    LANG_LOGIC_RESULT_UNKNOWN,         /**< Cannot determine */
    LANG_LOGIC_RESULT_INCONSISTENT,    /**< Premises inconsistent */
    LANG_LOGIC_RESULT_TIMEOUT,         /**< Reasoning timed out */
    LANG_LOGIC_RESULT_ERROR            /**< Error occurred */
} language_logic_result_t;

/**
 * @brief Logical query
 */
typedef struct {
    language_logic_operation_t operation; /**< Operation type */
    const char* premise_a;               /**< First premise (text) */
    const char* premise_b;               /**< Second premise (if needed) */
    const char* query;                   /**< Query to evaluate */
    uint32_t max_depth;                  /**< Max inference depth */
    uint32_t timeout_ms;                 /**< Timeout for reasoning */
    float min_confidence;                /**< Minimum confidence */
} language_logic_query_t;

/**
 * @brief Logical inference result
 */
typedef struct {
    language_logic_result_t result;     /**< Result type */
    float confidence;                    /**< Confidence [0-1] */
    uint32_t inference_steps;            /**< Steps used */
    uint32_t time_ms;                    /**< Time taken */
    char explanation[256];               /**< Human-readable explanation */
    bool has_proof;                      /**< Formal proof available */
} language_logic_inference_t;

/**
 * @brief Discourse consistency state
 */
typedef struct {
    bool is_consistent;                  /**< Discourse is consistent */
    uint32_t contradiction_count;        /**< Number of contradictions */
    uint32_t unresolved_references;      /**< Unresolved references */
    uint32_t failed_presuppositions;     /**< Failed presuppositions */
    float coherence_score;               /**< Overall coherence [0-1] */
} language_discourse_state_t;

/**
 * @brief Bridge configuration
 */
#ifndef LANGUAGE_LOGIC_CONFIG_T_DEFINED
#define LANGUAGE_LOGIC_CONFIG_T_DEFINED
typedef struct {
    /* Enable flags */
    bool enable_entailment_checking;   /**< Check semantic entailment */
    bool enable_consistency_checking;  /**< Check discourse consistency */
    bool enable_presupposition_check;  /**< Verify presuppositions */
    bool enable_reference_resolution;  /**< Logical reference binding */
    bool enable_implicature_reasoning; /**< Reason about implicatures */

    /* Reasoning parameters */
    uint32_t max_inference_depth;      /**< Maximum reasoning depth */
    uint32_t default_timeout_ms;       /**< Default reasoning timeout */
    float min_confidence_threshold;    /**< Minimum confidence to accept */

    /* Performance */
    bool enable_caching;               /**< Cache inference results */
    uint32_t cache_size;               /**< Inference cache size */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    uint32_t update_interval_ms;       /**< Update cycle interval */
} language_logic_config_t;
#endif /* LANGUAGE_LOGIC_CONFIG_T_DEFINED */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t entailment_checks;        /**< Entailment checks performed */
    uint64_t consistency_checks;       /**< Consistency checks */
    uint64_t contradictions_found;     /**< Contradictions detected */
    uint64_t inferences_made;          /**< Total inferences */
    uint64_t cache_hits;               /**< Cache hits */
    uint64_t timeouts;                 /**< Reasoning timeouts */
    float avg_inference_time_ms;       /**< Average inference time */
    float avg_confidence;              /**< Average confidence */
} language_logic_stats_t;

/**
 * @brief Bridge state
 */
struct language_logic_bridge {
    language_logic_config_t config;
    bool initialized;
    bool active;

    language_orchestrator_t* orchestrator;
    symbolic_logic_t* logic_engine;

    language_discourse_state_t discourse_state;

    language_logic_stats_t stats;
    uint64_t last_update_us;
};

typedef struct language_logic_bridge language_logic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

void language_logic_default_config(language_logic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

language_logic_bridge_t* language_logic_bridge_create(
    language_orchestrator_t* orchestrator,
    const language_logic_config_t* config);

void language_logic_bridge_destroy(language_logic_bridge_t* bridge);

int language_logic_bridge_connect_logic_engine(
    language_logic_bridge_t* bridge,
    symbolic_logic_t* logic_engine);

//=============================================================================
// Inference API
//=============================================================================

int language_logic_bridge_check_entailment(
    language_logic_bridge_t* bridge,
    const char* premise,
    const char* hypothesis,
    language_logic_inference_t* result);

int language_logic_bridge_check_consistency(
    language_logic_bridge_t* bridge,
    const char** statements,
    uint32_t count,
    language_logic_inference_t* result);

int language_logic_bridge_resolve_reference(
    language_logic_bridge_t* bridge,
    const char* text,
    const char* reference,
    char* resolved,
    uint32_t resolved_size);

int language_logic_bridge_query(
    language_logic_bridge_t* bridge,
    const language_logic_query_t* query,
    language_logic_inference_t* result);

//=============================================================================
// Discourse API
//=============================================================================

int language_logic_bridge_add_to_discourse(
    language_logic_bridge_t* bridge,
    const char* statement);

int language_logic_bridge_get_discourse_state(
    const language_logic_bridge_t* bridge,
    language_discourse_state_t* state);

int language_logic_bridge_clear_discourse(
    language_logic_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

int language_logic_bridge_get_stats(
    const language_logic_bridge_t* bridge,
    language_logic_stats_t* stats);

void language_logic_bridge_reset_stats(language_logic_bridge_t* bridge);

//=============================================================================
// String Conversion
//=============================================================================

const char* language_logic_operation_to_string(language_logic_operation_t op);
const char* language_logic_result_to_string(language_logic_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_LOGIC_BRIDGE_H */
