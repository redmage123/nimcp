/**
 * @file nimcp_wernicke_quantum_bridge.h
 * @brief Quantum-Accelerated Language Comprehension
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Integrates quantum algorithms with Wernicke's area
 * WHY:  Accelerate semantic search, disambiguation, and spreading activation
 * HOW:  Quantum walks for spreading activation, Grover for lexicon search
 *
 * BIOLOGICAL INSPIRATION:
 * =======================
 * Wernicke's area processes language in massively parallel fashion:
 * - Simultaneous activation of multiple word candidates
 * - Parallel semantic spreading across concept network
 * - Rapid disambiguation through context integration
 *
 * QUANTUM CONCEPTS:
 * =================
 *
 * 1. QUANTUM WALKS FOR SEMANTIC SPREADING:
 *    - Random walks on semantic network become quantum walks
 *    - Quadratic speedup: O(sqrt(N)) vs O(N) mixing time
 *    - Interference patterns enhance relevant concepts
 *
 * 2. GROVER SEARCH FOR LEXICON:
 *    - Find matching word in O(sqrt(N)) vs O(N)
 *    - Oracle: semantic/phonological match criteria
 *    - Amplitude amplification of correct words
 *
 * 3. QUANTUM DISAMBIGUATION:
 *    - Superposition of word senses
 *    - Context collapses to most likely meaning
 *    - Bayesian-like probability concentration
 *
 * 4. QMC FOR PROBABILISTIC COMPREHENSION:
 *    - Monte Carlo sampling of parse trees
 *    - Quantum acceleration of belief updates
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WERNICKE_QUANTUM_BRIDGE_H
#define NIMCP_WERNICKE_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct wernicke_quantum_bridge wernicke_quantum_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum quantum walk steps */
#define WERNICKE_QW_MAX_STEPS              100

/** @brief Maximum Grover iterations */
#define WERNICKE_GROVER_MAX_ITERATIONS     20

/** @brief Maximum concepts in quantum superposition */
#define WERNICKE_MAX_SUPERPOSITION         64

/** @brief Default semantic search speedup target */
#define WERNICKE_TARGET_SPEEDUP            4.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Quantum algorithm type
 */
typedef enum {
    WERNICKE_QA_GROVER = 0,       /**< Grover search */
    WERNICKE_QA_QUANTUM_WALK,     /**< Quantum walk */
    WERNICKE_QA_QMC,              /**< Quantum Monte Carlo */
    WERNICKE_QA_AMPLITUDE_EST,    /**< Amplitude estimation */
    WERNICKE_QA_HYBRID            /**< Classical-quantum hybrid */
} wernicke_quantum_algo_t;

/**
 * @brief Search target type
 */
typedef enum {
    WERNICKE_SEARCH_PHONEME = 0,  /**< Phoneme pattern match */
    WERNICKE_SEARCH_WORD,         /**< Word in lexicon */
    WERNICKE_SEARCH_CONCEPT,      /**< Concept in semantic network */
    WERNICKE_SEARCH_RELATION      /**< Semantic relation */
} wernicke_search_target_t;

/**
 * @brief Disambiguation mode
 */
typedef enum {
    WERNICKE_DISAMBIG_FREQUENCY = 0, /**< Most frequent sense */
    WERNICKE_DISAMBIG_CONTEXT,       /**< Context-weighted */
    WERNICKE_DISAMBIG_QUANTUM,       /**< Quantum amplitude */
    WERNICKE_DISAMBIG_HYBRID         /**< Combined approach */
} wernicke_disambig_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Quantum state for concept superposition
 */
typedef struct {
    uint32_t* concept_ids;        /**< Concept IDs in superposition */
    float* amplitudes;            /**< Complex amplitudes (real part) */
    float* phases;                /**< Complex amplitudes (phase) */
    uint32_t num_concepts;        /**< Number of concepts */
    float total_probability;      /**< Sum of |amplitude|^2 */
    bool collapsed;               /**< Whether state has collapsed */
    uint32_t collapsed_id;        /**< ID after collapse */
} quantum_concept_state_t;

/**
 * @brief Quantum walk state
 */
typedef struct {
    uint32_t current_node;        /**< Current position in graph */
    float* node_amplitudes;       /**< Amplitude at each node */
    uint32_t num_nodes;           /**< Graph size */
    uint32_t steps_taken;         /**< Walk steps so far */
    float mixing_progress;        /**< Mixing completion [0-1] */
} quantum_walk_state_t;

/**
 * @brief Grover search result
 */
typedef struct {
    uint32_t found_id;            /**< Found item ID */
    char found_word[64];          /**< Found word (if word search) */
    float probability;            /**< Success probability */
    uint32_t iterations_used;     /**< Grover iterations */
    float speedup_achieved;       /**< Speedup vs classical */
    bool success;                 /**< Search successful */
} quantum_search_result_t;

/**
 * @brief Disambiguation result
 */
typedef struct {
    uint32_t sense_id;            /**< Selected sense ID */
    float confidence;             /**< Selection confidence */
    uint32_t* alternatives;       /**< Alternative senses */
    float* alt_probabilities;     /**< Alternative probabilities */
    uint32_t num_alternatives;    /**< Number of alternatives */
    wernicke_disambig_mode_t mode_used; /**< Algorithm used */
} quantum_disambig_result_t;

/**
 * @brief Spreading activation result
 */
typedef struct {
    uint32_t* activated_concepts; /**< Activated concept IDs */
    float* activation_levels;     /**< Activation strengths */
    uint32_t num_activated;       /**< Number activated */
    float total_activation;       /**< Total activation mass */
    uint32_t walk_steps;          /**< Quantum walk steps */
    float speedup_achieved;       /**< Speedup vs classical */
} quantum_spreading_result_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Algorithm selection */
    wernicke_quantum_algo_t default_algo;     /**< Default algorithm */
    bool enable_hybrid;                        /**< Allow hybrid mode */
    float classical_fallback_threshold;        /**< When to use classical */

    /* Grover parameters */
    uint32_t grover_max_iterations;           /**< Max Grover iterations */
    float grover_success_threshold;           /**< Min success probability */

    /* Quantum walk parameters */
    uint32_t walk_max_steps;                  /**< Max walk steps */
    float walk_mixing_threshold;              /**< Mixing completion threshold */
    bool walk_use_continuous;                 /**< Continuous-time walk */

    /* Disambiguation */
    wernicke_disambig_mode_t disambig_mode;   /**< Disambiguation mode */
    float context_weight;                     /**< Context influence weight */

    /* Performance */
    bool enable_speedup_tracking;             /**< Track speedup metrics */
    float target_speedup;                     /**< Target speedup factor */

    /* Integration */
    bool enable_logging;                      /**< Enable logging */
} wernicke_quantum_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t grover_searches;     /**< Total Grover searches */
    uint64_t quantum_walks;       /**< Total quantum walks */
    uint64_t disambiguations;     /**< Total disambiguations */
    uint64_t spreading_activations; /**< Total spreading activations */
    float avg_grover_speedup;     /**< Average Grover speedup */
    float avg_walk_speedup;       /**< Average walk speedup */
    float avg_disambiguation_confidence; /**< Avg confidence */
    uint64_t successful_searches; /**< Successful searches */
    uint64_t failed_searches;     /**< Failed searches */
    float max_speedup_achieved;   /**< Best speedup achieved */
} wernicke_quantum_stats_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_default_config(wernicke_quantum_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create quantum bridge
 *
 * @param wernicke Wernicke adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wernicke_quantum_bridge_t* wernicke_quantum_bridge_create(
    void* wernicke,
    const wernicke_quantum_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void wernicke_quantum_bridge_destroy(wernicke_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum acceleration is enabled
 *
 * @param bridge Bridge handle
 * @return true if enabled
 */
bool wernicke_quantum_is_enabled(const wernicke_quantum_bridge_t* bridge);

/**
 * @brief Enable/disable quantum acceleration
 *
 * @param bridge Bridge handle
 * @param enabled Enable flag
 */
void wernicke_quantum_set_enabled(wernicke_quantum_bridge_t* bridge,
                                   bool enabled);

/* ============================================================================
 * Grover Search API
 * ============================================================================ */

/**
 * @brief Search lexicon using Grover's algorithm
 *
 * @param bridge Bridge handle
 * @param phoneme_pattern Phoneme pattern to match
 * @param pattern_len Pattern length
 * @param lexicon_size Size of lexicon
 * @param result Output search result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) classical
 */
int wernicke_quantum_search_lexicon(wernicke_quantum_bridge_t* bridge,
                                     const uint8_t* phoneme_pattern,
                                     uint32_t pattern_len,
                                     uint32_t lexicon_size,
                                     quantum_search_result_t* result);

/**
 * @brief Search semantic network for concept
 *
 * @param bridge Bridge handle
 * @param semantic_target Target semantic vector
 * @param target_dim Vector dimension
 * @param num_concepts Number of concepts to search
 * @param result Output search result
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_search_concepts(wernicke_quantum_bridge_t* bridge,
                                      const float* semantic_target,
                                      uint32_t target_dim,
                                      uint32_t num_concepts,
                                      quantum_search_result_t* result);

/* ============================================================================
 * Quantum Walk API
 * ============================================================================ */

/**
 * @brief Initialize quantum walk on semantic graph
 *
 * @param bridge Bridge handle
 * @param start_concept Starting concept ID
 * @param graph_size Number of nodes in graph
 * @param state Output walk state
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_walk_init(wernicke_quantum_bridge_t* bridge,
                                uint32_t start_concept,
                                uint32_t graph_size,
                                quantum_walk_state_t* state);

/**
 * @brief Take quantum walk step
 *
 * @param bridge Bridge handle
 * @param state Walk state (modified in place)
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_walk_step(wernicke_quantum_bridge_t* bridge,
                                quantum_walk_state_t* state);

/**
 * @brief Run quantum walk until mixing
 *
 * @param bridge Bridge handle
 * @param state Walk state
 * @param max_steps Maximum steps
 * @return Steps taken, or -1 on error
 */
int wernicke_quantum_walk_run(wernicke_quantum_bridge_t* bridge,
                               quantum_walk_state_t* state,
                               uint32_t max_steps);

/**
 * @brief Measure quantum walk result
 *
 * @param bridge Bridge handle
 * @param state Walk state
 * @param result Output spreading result
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_walk_measure(wernicke_quantum_bridge_t* bridge,
                                   const quantum_walk_state_t* state,
                                   quantum_spreading_result_t* result);

/* ============================================================================
 * Disambiguation API
 * ============================================================================ */

/**
 * @brief Disambiguate word sense using quantum approach
 *
 * @param bridge Bridge handle
 * @param word Word to disambiguate
 * @param context_embedding Context vector
 * @param context_dim Context dimension
 * @param result Output disambiguation result
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_disambiguate(wernicke_quantum_bridge_t* bridge,
                                   const char* word,
                                   const float* context_embedding,
                                   uint32_t context_dim,
                                   quantum_disambig_result_t* result);

/**
 * @brief Create quantum superposition of word senses
 *
 * @param bridge Bridge handle
 * @param word Word with multiple senses
 * @param state Output quantum state
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_superpose_senses(wernicke_quantum_bridge_t* bridge,
                                       const char* word,
                                       quantum_concept_state_t* state);

/**
 * @brief Collapse superposition based on context
 *
 * @param bridge Bridge handle
 * @param state Quantum state to collapse
 * @param context_embedding Context vector
 * @param context_dim Context dimension
 * @return Collapsed concept ID, or -1 on error
 */
int32_t wernicke_quantum_collapse(wernicke_quantum_bridge_t* bridge,
                                   quantum_concept_state_t* state,
                                   const float* context_embedding,
                                   uint32_t context_dim);

/* ============================================================================
 * Spreading Activation API
 * ============================================================================ */

/**
 * @brief Quantum-accelerated spreading activation
 *
 * @param bridge Bridge handle
 * @param seed_concepts Initial concept IDs
 * @param seed_activations Initial activation levels
 * @param num_seeds Number of seed concepts
 * @param graph_size Total concepts in network
 * @param result Output spreading result
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_spreading_activation(
    wernicke_quantum_bridge_t* bridge,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    uint32_t graph_size,
    quantum_spreading_result_t* result);

/**
 * @brief Get related concepts via quantum walk
 *
 * @param bridge Bridge handle
 * @param concept_id Source concept
 * @param max_related Maximum related concepts
 * @param related_ids Output related concept IDs
 * @param similarities Output similarity scores
 * @param num_related Output actual count
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_get_related(wernicke_quantum_bridge_t* bridge,
                                  uint32_t concept_id,
                                  uint32_t max_related,
                                  uint32_t* related_ids,
                                  float* similarities,
                                  uint32_t* num_related);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_get_stats(const wernicke_quantum_bridge_t* bridge,
                                wernicke_quantum_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void wernicke_quantum_reset_stats(wernicke_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int wernicke_quantum_get_config(const wernicke_quantum_bridge_t* bridge,
                                 wernicke_quantum_config_t* config);

/**
 * @brief Estimate speedup for given problem size
 *
 * @param bridge Bridge handle
 * @param problem_size Size of search/walk space
 * @param algo Algorithm to use
 * @return Estimated speedup factor
 */
float wernicke_quantum_estimate_speedup(const wernicke_quantum_bridge_t* bridge,
                                         uint32_t problem_size,
                                         wernicke_quantum_algo_t algo);

/* ============================================================================
 * Memory Management API
 * ============================================================================ */

/**
 * @brief Free quantum concept state
 *
 * @param state State to free
 */
void wernicke_quantum_state_free(quantum_concept_state_t* state);

/**
 * @brief Free quantum walk state
 *
 * @param state State to free
 */
void wernicke_quantum_walk_free(quantum_walk_state_t* state);

/**
 * @brief Free disambiguation result
 *
 * @param result Result to free
 */
void wernicke_quantum_disambig_free(quantum_disambig_result_t* result);

/**
 * @brief Free spreading result
 *
 * @param result Result to free
 */
void wernicke_quantum_spreading_free(quantum_spreading_result_t* result);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* wernicke_quantum_algo_to_string(wernicke_quantum_algo_t algo);
const char* wernicke_quantum_target_to_string(wernicke_search_target_t target);
const char* wernicke_quantum_disambig_to_string(wernicke_disambig_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_QUANTUM_BRIDGE_H */
