/**
 * @file nimcp_reasoning_chain.h
 * @brief Multi-step reasoning chain engine for human-like reasoning
 *
 * WHAT: Orchestrates cognitive modules to perform multi-step reasoning chains
 * WHY:  Enable deliberative, transparent reasoning with traceable steps
 * HOW:  Connects to brain subsystems (engram, knowledge, working memory,
 *        predictive coding, epistemic filter, RCOG, JEPA, world model)
 *        and executes a 9-phase pipeline:
 *        recall -> knowledge -> decompose -> world_model -> infer ->
 *        jepa_predict -> verify -> assess -> synthesize
 *
 * BIOLOGICAL BASIS:
 * Models the prefrontal cortex orchestrating multi-step reasoning by:
 * - Retrieving relevant memories (hippocampus/engram)
 * - Querying stored knowledge (semantic memory)
 * - Maintaining intermediate results in working memory (dlPFC)
 * - Verifying consistency via predictive coding (hierarchical prediction)
 * - Assessing epistemic quality (anterior cingulate cortex)
 * - Synthesizing a coherent conclusion (integration)
 *
 * DESIGN PRINCIPLES:
 * - Graceful degradation: skip unavailable modules, never crash
 * - Transparent reasoning: every step is logged with confidence/relevance
 * - Configurable: enable/disable each phase independently
 * - Domain-aware: can restrict reasoning to a knowledge domain
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#ifndef NIMCP_REASONING_CHAIN_H
#define NIMCP_REASONING_CHAIN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* Forward-declare brain_t to avoid header dependency cycles */
struct brain_struct;
typedef struct brain_struct* brain_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Default maximum recursion depth for decomposition */
#define REASONING_CHAIN_DEFAULT_MAX_DEPTH 10

/** Default maximum reasoning steps per query */
#define REASONING_CHAIN_DEFAULT_MAX_STEPS 50

/** Default confidence threshold to stop reasoning early */
#define REASONING_CHAIN_DEFAULT_CONFIDENCE_THRESHOLD 0.8f

/** Default uncertainty threshold to flag uncertainty */
#define REASONING_CHAIN_DEFAULT_UNCERTAINTY_THRESHOLD 0.7f

/** Default working memory slots for active reasoning */
#define REASONING_CHAIN_DEFAULT_WM_SLOTS 7

/** Initial capacity for chain step array */
#define REASONING_CHAIN_INITIAL_CAPACITY 16

/** Maximum description length for a reasoning step */
#define REASONING_STEP_DESC_LEN 512

/** Maximum conclusion length for a reasoning chain */
#define REASONING_CHAIN_CONCLUSION_LEN 1024

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Type of reasoning step in the chain
 *
 * WHAT: Categorizes each step in the reasoning pipeline
 * WHY:  Enables traceability and analysis of the reasoning process
 */
typedef enum {
    REASONING_STEP_RECALL,           /**< Memory/engram recall */
    REASONING_STEP_KNOWLEDGE,        /**< Knowledge system query */
    REASONING_STEP_INFERENCE,        /**< Logical inference from premises */
    REASONING_STEP_VERIFICATION,     /**< Predictive coding check */
    REASONING_STEP_UNCERTAINTY,      /**< Epistemic assessment */
    REASONING_STEP_ANALOGY,          /**< Analogical reasoning */
    REASONING_STEP_DECOMPOSITION,    /**< Break problem into sub-parts */
    REASONING_STEP_SYNTHESIS,        /**< Combine sub-results */
    REASONING_STEP_WORLD_MODEL,      /**< World model causal simulation */
    REASONING_STEP_JEPA_PREDICTION,  /**< JEPA latent-space consistency check */
    REASONING_STEP_SYMBOLIC_LOGIC,   /**< Symbolic logic formal inference */

    /* Convergent architecture step types (added v2.6.4) */
    REASONING_STEP_SEMANTIC_ACTIVATION,  /**< Semantic memory concept spreading */
    REASONING_STEP_HIPPOCAMPAL_RECALL,   /**< Hippocampal pattern completion */
    REASONING_STEP_MATHEMATICAL,         /**< Parietal mathematical/spatial */
    REASONING_STEP_INTUITIVE,            /**< Rapid intuitive hunch */
    REASONING_STEP_CREATIVE_ANALOGY,     /**< Cross-domain creative analogy */
    REASONING_STEP_SELF_KNOWLEDGE,       /**< Internal KG self-knowledge */
    REASONING_STEP_NEURAL_LOGIC,         /**< Neural logic gate inference */
    REASONING_STEP_MESH_CONSENSUS,       /**< Distributed mesh consensus */
    REASONING_STEP_MODULATION,           /**< Confidence modulation step */

    /* Metacognitive controller step type (added v2.6.4) */
    REASONING_STEP_METACOGNITIVE,        /**< Metacognitive strategy assessment */

    /* Abductive reasoning step type (added v2.6.4) */
    REASONING_STEP_ABDUCTIVE,            /**< Abductive hypothesis (inference to best explanation) */

    /* Affective modulation step type (added v2.6.4) */
    REASONING_STEP_AFFECTIVE,            /**< Affective emotion-based modulation */

    /* Causal reasoning step type (added v2.6.4) */
    REASONING_STEP_CAUSAL,               /**< Causal DAG inference (do-calculus) */

    /* Visuospatial reasoning step type (added v2.6.4) */
    REASONING_STEP_VISUOSPATIAL          /**< Visuospatial/parietal spatial reasoning */
} reasoning_step_type_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Single step in a reasoning chain
 *
 * WHAT: One discrete reasoning operation with metadata
 * WHY:  Provide full traceability of the reasoning process
 * HOW:  Populated during each phase of the reasoning pipeline
 */
typedef struct {
    uint32_t step_id;                           /**< Sequential step number */
    reasoning_step_type_t type;                 /**< What kind of step */
    char description[REASONING_STEP_DESC_LEN];  /**< Human-readable description */
    float confidence;                           /**< Step confidence [0-1] */
    float relevance;                            /**< Relevance to query [0-1] */
    uint64_t timestamp_us;                      /**< When step was executed */
} reasoning_step_t;

/**
 * @brief Complete reasoning chain from query to conclusion
 *
 * WHAT: Ordered sequence of reasoning steps with final conclusion
 * WHY:  Represents the full reasoning trace for a query
 * HOW:  Dynamically growing array of steps, plus synthesis metadata
 */
typedef struct {
    reasoning_step_t* steps;                         /**< Dynamic array of steps */
    uint32_t num_steps;                              /**< Current number of steps */
    uint32_t capacity;                               /**< Allocated capacity */
    char conclusion[REASONING_CHAIN_CONCLUSION_LEN]; /**< Final synthesized answer */
    float overall_confidence;                        /**< Geometric mean of step confidences */
    float uncertainty_score;                         /**< Epistemic uncertainty [0-1] */
    bool is_complete;                                /**< Has synthesis been performed */
    bool has_uncertainty_flag;                        /**< Uncertainty above threshold */
    uint64_t start_time_us;                          /**< When reasoning began */
    uint64_t end_time_us;                            /**< When reasoning finished */
} reasoning_chain_t;

/**
 * @brief Configuration for the reasoning engine
 *
 * WHAT: Tuneable parameters for the reasoning pipeline
 * WHY:  Allow domain-specific tuning of reasoning behavior
 * HOW:  Struct with defaults via reasoning_engine_default_config()
 */
typedef struct {
    uint32_t max_depth;           /**< Max recursion depth (default 10) */
    uint32_t max_steps;           /**< Max reasoning steps (default 50) */
    float confidence_threshold;    /**< Stop when confidence exceeds this (default 0.8) */
    float uncertainty_threshold;   /**< Flag uncertainty above this (default 0.7) */
    bool enable_engram_recall;     /**< Use engram system (default true) */
    bool enable_knowledge_query;   /**< Use knowledge system (default true) */
    bool enable_predictive_verify; /**< Verify steps with predictive coding (default true) */
    bool enable_epistemic_check;   /**< Check for biases (default true) */
    bool enable_analogical;        /**< Enable analogical reasoning (default true) */
    bool enable_working_memory;    /**< Use working memory (default true) */
    bool enable_world_model;       /**< Use world model for causal simulation (default true) */
    bool enable_jepa_prediction;   /**< Use JEPA for latent-space verification (default true) */
    bool enable_symbolic_logic;    /**< Use symbolic logic for formal inference (default true) */
    bool enable_concurrent_pipeline; /**< Run independent phases in parallel (default true) */
    uint32_t symbolic_inference_depth; /**< Max backward chain depth (default 10) */
    uint32_t working_memory_slots; /**< WM capacity (default 7) */
    uint32_t world_model_horizon;  /**< World model simulation steps (default 3) */
    uint32_t concurrent_pool_size; /**< Thread pool size for concurrent pipeline (default 4) */

    /* Convergent reasoning architecture (added v2.6.4) */
    bool enable_convergent_reasoning;    /**< Use convergent evidence accumulation (default true) */
    uint32_t convergent_pool_size;       /**< Thread pool size for convergent pipeline (default 8) */
    uint32_t max_convergent_contributors; /**< Max active contributors (default 64) */
    float convergence_ema_alpha;          /**< EMA smoothing factor (default 0.3) */
    float convergence_threshold;          /**< EMA delta below this = converged (default 0.005) */
    uint32_t convergence_timeout_ms;      /**< Max time for convergence (default 500ms) */

    /* Confidence calibration (added v2.6.4) */
    bool enable_calibration;              /**< Enable confidence calibration learning (default false) */
    float calibration_learning_rate;      /**< Calibration EMA learning rate (default 0.05) */

    /* Metacognitive controller (added v2.6.4) */
    bool enable_metacognition;            /**< Use metacognitive strategy selection (default true) */

    /* Abductive reasoning (added v2.6.4) */
    bool enable_abductive_reasoning;      /**< Use abductive hypothesis generation (default true) */

    /* Affective modulation (added v2.6.4) */
    bool enable_affective_modulation;     /**< Use affective emotion-based modulation (default true) */

    /* Causal reasoning (added v2.6.4) */
    bool enable_causal_reasoning;         /**< Use causal DAG inference (default false, opt-in) */

    /* Visuospatial reasoning (added v2.6.4) */
    bool enable_visuospatial_reasoning;   /**< Use visuospatial reasoning (default false, opt-in) */
} reasoning_engine_config_t;

/**
 * @brief Statistics for the reasoning engine
 *
 * WHAT: Aggregate metrics across all queries processed
 * WHY:  Monitor reasoning engine performance and quality
 */
typedef struct {
    uint32_t total_queries;         /**< Total queries processed */
    uint32_t successful_queries;    /**< Queries that completed successfully */
    uint32_t total_steps;           /**< Total reasoning steps across all queries */
    float avg_confidence;           /**< Running average of overall confidence */
    float avg_steps_per_query;      /**< Running average of steps per query */
    uint32_t engram_recalls;        /**< Times engram recall was performed */
    uint32_t knowledge_queries;     /**< Times knowledge system was queried */
    uint32_t verification_passes;    /**< Predictive verification successes */
    uint32_t verification_failures;  /**< Predictive verification failures */
    uint32_t uncertainty_flags;      /**< Times uncertainty was flagged */
    uint32_t world_model_simulations;/**< Times world model was invoked */
    uint32_t jepa_predictions;       /**< Times JEPA predictor was invoked */
    uint32_t symbolic_queries;       /**< Times symbolic logic KB was queried */
    uint32_t symbolic_proofs;        /**< Times symbolic backward chain proved goal */

    /* Convergent reasoning stats (added v2.6.4) */
    uint32_t convergent_queries;          /**< Total convergent reasoning queries */
    float avg_convergent_contributors;    /**< Average active contributors per query */
    float avg_convergence_time_us;        /**< Average convergence time in microseconds */

    /* Confidence calibration stats (added v2.6.4) */
    float avg_calibration_reliability;    /**< Average contributor reliability from calibration */

    /* Metacognitive controller stats (added v2.6.4) */
    uint32_t metacognitive_assessments;   /**< Total metacognitive assessments */
    float avg_assessment_accuracy;        /**< Average metacognitive accuracy [0-1] */

    /* Abductive reasoning stats (added v2.6.4) */
    uint32_t abductive_queries;           /**< Total abductive reasoning queries */
    float avg_hypotheses_per_query;       /**< Average hypotheses generated per query */

    /* Affective modulation stats (added v2.6.4) */
    uint32_t affective_modulations;       /**< Total affective modulations applied */
    float avg_affective_delta;            /**< Average affective confidence delta */

    /* Causal reasoning stats (added v2.6.4) */
    uint32_t causal_queries;              /**< Total causal reasoning queries */
    float avg_causal_strength;            /**< Average causal strength from queries */

    /* Visuospatial reasoning stats (added v2.6.4) */
    uint32_t visuospatial_queries;        /**< Total visuospatial reasoning queries */
} reasoning_engine_stats_t;

/**
 * @brief Reasoning engine instance (opaque in header, defined in .c)
 */
typedef struct reasoning_engine reasoning_engine_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Get default reasoning engine configuration
 *
 * WHAT: Return configuration with sensible defaults
 * WHY:  Simplify engine creation with proven parameters
 * HOW:  Static initialization with constant values
 *
 * @return Default configuration struct
 *
 * COMPLEXITY: O(1)
 */
reasoning_engine_config_t reasoning_engine_default_config(void);

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create a reasoning engine
 *
 * WHAT: Allocate and initialize a reasoning engine instance
 * WHY:  Required before any reasoning operations
 * HOW:  Allocate struct, copy config, zero statistics
 *
 * @param config Engine configuration (NULL for defaults)
 * @return Engine instance or NULL on allocation failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~200 bytes
 */
reasoning_engine_t* reasoning_engine_create(const reasoning_engine_config_t* config);

/**
 * @brief Destroy a reasoning engine
 *
 * WHAT: Free all engine resources
 * WHY:  Prevent memory leaks
 * HOW:  Free engine struct (does NOT destroy connected brain modules)
 *
 * @param engine Engine to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 */
void reasoning_engine_destroy(reasoning_engine_t* engine);

/*=============================================================================
 * BRAIN CONNECTION
 *===========================================================================*/

/**
 * @brief Connect engine to a brain instance
 *
 * WHAT: Extract module pointers from brain for use during reasoning
 * WHY:  Reasoning engine operates on brain's cognitive subsystems
 * HOW:  Call brain accessor functions for each subsystem, store pointers
 *
 * @param engine Reasoning engine
 * @param brain Brain instance to connect
 * @return 0 on success, -1 on error (NULL engine)
 *
 * NOTE: Individual modules may be NULL (brain not configured with them).
 *       The engine gracefully skips unavailable modules during reasoning.
 *
 * COMPLEXITY: O(1)
 */
int reasoning_engine_connect_brain(reasoning_engine_t* engine, brain_t brain);

/*=============================================================================
 * CORE REASONING
 *===========================================================================*/

/**
 * @brief Perform multi-step reasoning on a query
 *
 * WHAT: Execute the full reasoning pipeline for a natural language query
 * WHY:  Core function — orchestrates all cognitive modules into a chain
 * HOW:  recall -> knowledge -> decompose -> infer -> verify -> assess -> synthesize
 *
 * PIPELINE:
 * 1. Initialize chain, store query in working memory
 * 2. Recall: Hash query to feature vector, call engram_recall()
 * 3. Knowledge: Call knowledge_retrieve() and knowledge_find_connections()
 * 4. Decomposition: Analyze query type (what/why/how/etc.)
 * 5. Inference: Combine evidence, compute weighted confidence
 * 6. Verification: Use predictive_forward() to minimize free energy
 * 7. Epistemic: Call epistemic_assess_claim() for bias detection
 * 8. Synthesis: Geometric mean confidence, format conclusion
 *
 * @param engine Reasoning engine (must be connected to brain)
 * @param query Natural language query string
 * @param chain Output reasoning chain (caller must call reasoning_chain_cleanup)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(steps * module_cost) where module_cost varies
 */
int reasoning_engine_reason(reasoning_engine_t* engine, const char* query,
                            reasoning_chain_t* chain);

/**
 * @brief Perform reasoning restricted to a knowledge domain
 *
 * WHAT: Same as reasoning_engine_reason but filtered to one domain
 * WHY:  Focus reasoning on relevant knowledge area
 * HOW:  Sets domain context, then calls reasoning pipeline
 *
 * @param engine Reasoning engine
 * @param query Natural language query string
 * @param domain Knowledge domain to restrict to (see knowledge_domain_t)
 * @param chain Output reasoning chain
 * @return 0 on success, -1 on error
 */
int reasoning_engine_reason_in_domain(reasoning_engine_t* engine, const char* query,
                                      uint32_t domain, reasoning_chain_t* chain);

/*=============================================================================
 * CHAIN MANAGEMENT
 *===========================================================================*/

/**
 * @brief Initialize a reasoning chain
 *
 * WHAT: Zero-initialize a chain struct and allocate initial step array
 * WHY:  Prepare chain for step accumulation
 * HOW:  memset to zero, allocate initial capacity
 *
 * @param chain Chain to initialize (non-NULL)
 */
void reasoning_chain_init(reasoning_chain_t* chain);

/**
 * @brief Clean up a reasoning chain
 *
 * WHAT: Free dynamically allocated step array
 * WHY:  Prevent memory leaks after reasoning completes
 * HOW:  Free steps array, zero struct
 *
 * @param chain Chain to clean up (NULL safe)
 */
void reasoning_chain_cleanup(reasoning_chain_t* chain);

/**
 * @brief Add a step to a reasoning chain
 *
 * WHAT: Append a reasoning step, growing array if needed
 * WHY:  Build up the chain incrementally during reasoning
 * HOW:  Check capacity, realloc if needed, copy step
 *
 * @param chain Chain to add step to
 * @param step Step to add (deep copied)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1) amortized
 */
int reasoning_chain_add_step(reasoning_chain_t* chain, const reasoning_step_t* step);

/**
 * @brief Get a step from the chain by index
 *
 * @param chain Reasoning chain
 * @param index Step index [0, num_steps)
 * @return Pointer to step or NULL if invalid index
 */
const reasoning_step_t* reasoning_chain_get_step(const reasoning_chain_t* chain,
                                                  uint32_t index);

/**
 * @brief Get the overall confidence of the chain
 *
 * @param chain Reasoning chain
 * @return Overall confidence [0-1], or 0.0 if chain is NULL/empty
 */
float reasoning_chain_get_confidence(const reasoning_chain_t* chain);

/**
 * @brief Get the number of steps in the chain
 *
 * @param chain Reasoning chain
 * @return Number of steps, or 0 if chain is NULL
 */
uint32_t reasoning_chain_get_num_steps(const reasoning_chain_t* chain);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get engine statistics
 *
 * @param engine Reasoning engine
 * @param stats Output statistics struct
 * @return 0 on success, -1 on error
 */
int reasoning_engine_get_stats(const reasoning_engine_t* engine,
                               reasoning_engine_stats_t* stats);

/**
 * @brief Reset engine statistics to zero
 *
 * @param engine Reasoning engine
 * @return 0 on success, -1 on error
 */
int reasoning_engine_reset_stats(reasoning_engine_t* engine);

/*=============================================================================
 * UTILITY
 *===========================================================================*/

/**
 * @brief Get step type name as string
 *
 * @param type Step type
 * @return Static string name
 */
const char* reasoning_step_type_name(reasoning_step_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_CHAIN_H */
