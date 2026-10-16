/**
 * @file nimcp_broca_quantum_bridge.h
 * @brief Quantum-inspired language production optimization
 *
 * WHAT: Integrates quantum algorithms with Broca's region
 * WHY: Explore multiple sentence structures simultaneously for optimal expression
 * HOW: Quantum reasoning for syntax selection, lexical search, phoneme optimization
 *
 * BIOLOGICAL INSPIRATION:
 * - Broca's area explores multiple syntactic alternatives in parallel
 * - Working memory maintains superposition of lexical candidates
 * - Syntax selection resembles quantum collapse to optimal structure
 * - Phoneme sequencing benefits from parallel path evaluation
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Explore multiple syntactic structures simultaneously
 * - Grover search: Find optimal word from lexicon in O(sqrt(N))
 * - Interference: Cancel suboptimal sentence structures
 * - Amplitude amplification: Boost high-fluency expressions
 *
 * APPLICATIONS:
 * - Lexical selection: Find best word from semantic constraints
 * - Syntax optimization: Choose most fluent syntactic arrangement
 * - Phoneme sequencing: Optimize articulatory trajectory
 * - Error recovery: Find alternative expressions when blocked
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BROCA_QUANTUM_BRIDGE_H
#define NIMCP_BROCA_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct broca_quantum_bridge broca_quantum_bridge_t;

/**
 * @brief Quantum Broca configuration
 */
typedef struct {
    bool enabled;                    /**< Enable quantum optimization */
    uint32_t lexicon_search_depth;   /**< Max lexicon search depth (default: 1000) */
    uint32_t syntax_alternatives;    /**< Max parallel syntax structures (default: 8) */
    uint32_t max_grover_iterations;  /**< Max Grover iterations (default: 10) */
    float min_expression_confidence; /**< Min confidence for expression (default: 0.5) */
    bool enable_interference;        /**< Enable quantum interference (default: true) */
    bool use_superposition;          /**< Use superposition for alternatives (default: true) */
    uint32_t seed;                   /**< Random seed (default: 42) */
} broca_quantum_config_t;

/**
 * @brief Lexical candidate from quantum search
 */
typedef struct {
    uint32_t word_id;                /**< Word identifier */
    char word[64];                   /**< Word string */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float semantic_match;            /**< Semantic similarity [0, 1] */
    float frequency;                 /**< Word frequency [0, 1] */
    float combined_score;            /**< Combined selection score */
} quantum_lexical_candidate_t;

/**
 * @brief Lexical search result
 */
typedef struct {
    quantum_lexical_candidate_t* best_candidate; /**< Best word found */
    uint32_t candidates_evaluated;               /**< Total candidates */
    float satisfaction_probability;               /**< Search success probability */
    uint32_t grover_iterations_used;             /**< Grover iterations */
    float search_speedup;                         /**< Speedup vs linear search */
} quantum_lexical_result_t;

/**
 * @brief Syntactic structure candidate
 */
typedef struct {
    uint32_t structure_id;           /**< Structure identifier */
    char pattern[128];               /**< Syntactic pattern description */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float fluency_score;             /**< Predicted fluency [0, 1] */
    float complexity;                /**< Syntactic complexity [0, 1] */
    uint32_t word_count;             /**< Words in structure */
    bool is_grammatical;             /**< Grammar constraints satisfied */
} quantum_syntax_candidate_t;

/**
 * @brief Syntax optimization result
 */
typedef struct {
    quantum_syntax_candidate_t* best_structure; /**< Best syntax found */
    uint32_t structures_evaluated;              /**< Total structures */
    float satisfaction_probability;              /**< Optimization success */
    uint32_t grover_iterations_used;            /**< Grover iterations */
} quantum_syntax_result_t;

/**
 * @brief Phoneme sequence candidate
 */
typedef struct {
    uint32_t sequence_id;            /**< Sequence identifier */
    uint8_t phonemes[32];            /**< Phoneme sequence */
    uint32_t phoneme_count;          /**< Number of phonemes */
    float articulatory_cost;         /**< Motor effort [0, 1] */
    float coarticulation_score;      /**< Smoothness [0, 1] */
    float amplitude;                 /**< Quantum amplitude */
} quantum_phoneme_candidate_t;

/**
 * @brief Phoneme optimization result
 */
typedef struct {
    quantum_phoneme_candidate_t* best_sequence; /**< Best phoneme sequence */
    uint32_t sequences_evaluated;               /**< Total sequences */
    float optimization_score;                    /**< Overall quality */
} quantum_phoneme_result_t;

/**
 * @brief Statistics for quantum Broca operations
 */
typedef struct {
    uint64_t lexical_searches;       /**< Total lexical searches */
    uint64_t syntax_optimizations;   /**< Total syntax optimizations */
    uint64_t phoneme_optimizations;  /**< Total phoneme optimizations */
    float avg_lexical_speedup;       /**< Average lexical search speedup */
    float avg_syntax_speedup;        /**< Average syntax optimization speedup */
    float avg_satisfaction_prob;     /**< Average success probability */
    uint64_t successful_searches;    /**< Searches with high confidence */
    uint64_t failed_searches;        /**< Searches with low confidence */
} broca_quantum_stats_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default quantum Broca configuration
 * @return Default configuration
 */
broca_quantum_config_t broca_quantum_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create quantum Broca bridge
 * @param broca Broca adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
broca_quantum_bridge_t* broca_quantum_bridge_create(
    void* broca,
    const broca_quantum_config_t* config
);

/**
 * @brief Destroy quantum Broca bridge
 * @param bridge Bridge to destroy
 */
void broca_quantum_bridge_destroy(broca_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool broca_quantum_bridge_is_enabled(const broca_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum optimization
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void broca_quantum_bridge_set_enabled(broca_quantum_bridge_t* bridge, bool enabled);

//=============================================================================
// Lexical Search API
//=============================================================================

/**
 * @brief Search lexicon using quantum Grover algorithm
 * @param bridge Quantum bridge
 * @param semantic_target Target semantic vector (float array)
 * @param semantic_dim Dimension of semantic vector
 * @param lexicon_size Size of lexicon to search
 * @param result Output: search result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) linear search
 */
int broca_quantum_search_lexicon(
    broca_quantum_bridge_t* bridge,
    const float* semantic_target,
    uint32_t semantic_dim,
    uint32_t lexicon_size,
    quantum_lexical_result_t* result
);

//=============================================================================
// Syntax Optimization API
//=============================================================================

/**
 * @brief Optimize syntactic structure using quantum search
 * @param bridge Quantum bridge
 * @param semantic_content Semantic content to express
 * @param num_words Number of words to arrange
 * @param max_complexity Maximum syntactic complexity
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int broca_quantum_optimize_syntax(
    broca_quantum_bridge_t* bridge,
    const char* semantic_content,
    uint32_t num_words,
    float max_complexity,
    quantum_syntax_result_t* result
);

//=============================================================================
// Phoneme Optimization API
//=============================================================================

/**
 * @brief Optimize phoneme sequence for articulation
 * @param bridge Quantum bridge
 * @param target_phonemes Target phoneme sequence
 * @param phoneme_count Number of phonemes
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int broca_quantum_optimize_phonemes(
    broca_quantum_bridge_t* bridge,
    const uint8_t* target_phonemes,
    uint32_t phoneme_count,
    quantum_phoneme_result_t* result
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get quantum Broca statistics
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int broca_quantum_get_stats(
    const broca_quantum_bridge_t* bridge,
    broca_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Quantum bridge
 */
void broca_quantum_reset_stats(broca_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int broca_quantum_get_config(
    const broca_quantum_bridge_t* bridge,
    broca_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BROCA_QUANTUM_BRIDGE_H */
