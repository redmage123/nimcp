/**
 * @file nimcp_dialect_learning.h
 * @brief Neural Protocol Dialect Learning System
 *
 * WHAT: Learning system for communication dialects between brain regions/swarms
 * WHY:  Different modules develop specialized communication patterns ("dialects")
 * HOW:  Neural translation matrices learned from paired signal examples
 *
 * BIOLOGICAL MODEL:
 * ```
 * BIOLOGICAL COMMUNICATION             NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Brain region specialization       → Module-specific encodings
 * Cortical adaptation              → Dialect translation matrices
 * Cross-regional communication     → Learned signal mappings
 * Neural plasticity                → Online learning from examples
 * Polyglot regions (e.g., angular  → Bidirectional translation
 *   gyrus)
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════╗
 * ║              DIALECT LEARNING & TRANSLATION SYSTEM                 ║
 * ║  ┌─────────┐   ┌──────────────┐   ┌──────────────┐   ┌────────┐ ║
 * ║  │ Source  │→  │  Translation │→  │   Adapted    │→  │ Target │ ║
 * ║  │ Signal  │   │    Matrix    │   │    Signal    │   │ Region │ ║
 * ║  └─────────┘   └──────────────┘   └──────────────┘   └────────┘ ║
 * ║                        ↑                                          ║
 * ║                        │                                          ║
 * ║  ┌────────────────────────────────────────────┐                  ║
 * ║  │   Learning: Paired Examples (Source, Target) │                  ║
 * ║  │   Update matrices via gradient descent       │                  ║
 * ║  └────────────────────────────────────────────┘                  ║
 * ╚═══════════════════════════════════════════════════════════════════╝
 * ```
 *
 * KEY FEATURES:
 * - Automatic dialect discovery from paired signals
 * - Neural translation matrices (linear + non-linear options)
 * - Bidirectional learning (A→B and B→A)
 * - Compatibility scoring
 * - Multi-hop translation paths (A→B→C)
 * - Bio-async integration
 *
 * USAGE EXAMPLE:
 * ```c
 * // Initialize dialect learner
 * dialect_learner_config_t config = {
 *     .max_dialects = 100,
 *     .translation_dim = 64,
 *     .learning_rate = 0.01f,
 *     .enable_bidirectional = true,
 *     .enable_bio_async = true
 * };
 * dialect_learner_t* dl = dialect_learner_create(&config);
 *
 * // Learn from paired examples (V1 → MT visual motion translation)
 * uint32_t v1_id = 0x0700;  // Visual cortex V1
 * uint32_t mt_id = 0x0703;  // MT (motion area)
 * float* v1_signals[100];   // Array of V1 signals
 * float* mt_signals[100];   // Corresponding MT signals
 * dialect_learn_from_pairs(dl, v1_id, mt_id, v1_signals, mt_signals, 100, 64);
 *
 * // Translate V1 signal to MT dialect
 * float v1_signal[64];
 * float mt_signal[64];
 * uint32_t translated_size;
 * dialect_translate(dl, v1_id, mt_id, v1_signal, 64, mt_signal, &translated_size);
 *
 * // Check compatibility
 * float compat = dialect_get_compatibility(dl, v1_id, mt_id);
 * printf("V1→MT compatibility: %.2f\n", compat);
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 * - BBB security validation
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#ifndef NIMCP_DIALECT_LEARNING_H
#define NIMCP_DIALECT_LEARNING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dialect_learner_struct* dialect_learner_t;

//=============================================================================
// Dialect Structure
//=============================================================================

/**
 * @brief Neural dialect representation
 *
 * WHAT: Translation mapping between two communication styles
 * WHY:  Enable inter-module communication despite encoding differences
 * HOW:  Linear transformation matrix (can be extended to non-linear)
 */
typedef struct {
    uint32_t dialect_id;              /**< Unique dialect identifier */
    uint32_t source_region;           /**< Source brain region or swarm ID */
    uint32_t target_region;           /**< Target brain region or swarm ID */
    float* translation_matrix;        /**< Neural translation weights [rows x cols] */
    uint32_t matrix_rows;             /**< Output dimension */
    uint32_t matrix_cols;             /**< Input dimension */
    float compatibility_score;        /**< Translation quality (0.0-1.0) */
} neural_dialect_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for dialect learner
 *
 * WHAT: System parameters and limits
 * WHY:  Configure learning behavior and resource usage
 * HOW:  Set before initialization
 */
typedef struct {
    uint32_t max_dialects;          /**< Maximum dialect pairs to track */
    uint32_t translation_dim;       /**< Standard translation dimension */
    float learning_rate;            /**< Learning rate for updates */
    bool enable_bidirectional;      /**< Learn both A→B and B→A */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} dialect_learner_config_t;

//=============================================================================
// Core Functions
//=============================================================================

/**
 * @brief Create dialect learner system
 *
 * WHAT: Initialize dialect learning infrastructure
 * WHY:  Set up translation learning and storage
 * HOW:  Allocate structures, initialize matrices, register bio-async
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Dialect learner handle or NULL on failure
 */
NIMCP_EXPORT dialect_learner_t dialect_learner_create(
    const dialect_learner_config_t* config
);

/**
 * @brief Destroy dialect learner system
 *
 * WHAT: Clean up dialect learner resources
 * WHY:  Free all allocated memory
 * HOW:  Release dialects, matrices, unregister bio-async, free structure
 *
 * @param dl Dialect learner handle
 */
NIMCP_EXPORT void dialect_learner_destroy(dialect_learner_t dl);

//=============================================================================
// Dialect Learning
//=============================================================================

/**
 * @brief Learn dialect from paired signal examples
 *
 * WHAT: Train translation matrix from source→target examples
 * WHY:  Discover communication mapping between regions
 * HOW:  Gradient descent on linear transformation to minimize error
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @param source_signals Array of source signal examples
 * @param target_signals Array of corresponding target signals
 * @param num_pairs Number of example pairs
 * @param signal_size Size of each signal
 * @return 0 on success, negative on error
 *
 * LEARNING ALGORITHM:
 * 1. Initialize/retrieve translation matrix W
 * 2. For each pair (x_src, x_tgt):
 *    - Compute predicted: y = W * x_src
 *    - Compute error: e = x_tgt - y
 *    - Update: W += learning_rate * e * x_src^T
 * 3. Compute final compatibility score
 * 4. If bidirectional, also learn reverse mapping
 */
NIMCP_EXPORT int dialect_learn_from_pairs(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target,
    const float** source_signals,
    const float** target_signals,
    uint32_t num_pairs,
    uint32_t signal_size
);

/**
 * @brief Incrementally update dialect with single example
 *
 * WHAT: Online learning from one source→target pair
 * WHY:  Adapt to changing communication patterns
 * HOW:  Single gradient step on existing matrix
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @param source_signal Source signal
 * @param target_signal Corresponding target signal
 * @param signal_size Signal size
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_update_online(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target,
    const float* source_signal,
    const float* target_signal,
    uint32_t signal_size
);

//=============================================================================
// Translation
//=============================================================================

/**
 * @brief Translate signal from source to target dialect
 *
 * WHAT: Apply learned translation to convert signal
 * WHY:  Enable communication between different modules
 * HOW:  Matrix multiplication: translated = W * signal
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @param signal Input signal in source dialect
 * @param signal_size Size of input signal
 * @param translated Output translated signal (pre-allocated)
 * @param translated_size Output size of translated signal
 * @return 0 on success, negative on error
 *
 * NOTE: If no direct translation exists, attempts multi-hop path
 */
NIMCP_EXPORT int dialect_translate(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target,
    const float* signal,
    uint32_t signal_size,
    float* translated,
    uint32_t* translated_size
);

/**
 * @brief Translate with explicit dialect
 *
 * WHAT: Use specific dialect for translation
 * WHY:  Override automatic dialect selection
 * HOW:  Apply specified dialect matrix
 *
 * @param dl Dialect learner handle
 * @param dialect Dialect to use
 * @param signal Input signal
 * @param signal_size Signal size
 * @param translated Output translated signal
 * @param translated_size Output translated size
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_translate_with(
    dialect_learner_t dl,
    const neural_dialect_t* dialect,
    const float* signal,
    uint32_t signal_size,
    float* translated,
    uint32_t* translated_size
);

//=============================================================================
// Dialect Queries
//=============================================================================

/**
 * @brief Get dialect between two regions
 *
 * WHAT: Retrieve learned dialect
 * WHY:  Inspect translation mapping
 * HOW:  Look up in dialect table
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @param dialect Output dialect structure (pre-allocated)
 * @return 0 on success, negative if not found
 */
NIMCP_EXPORT int dialect_get(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target,
    neural_dialect_t* dialect
);

/**
 * @brief Check if dialect exists
 *
 * WHAT: Query dialect existence
 * WHY:  Test if translation is available
 * HOW:  Check dialect table
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @return true if dialect exists
 */
NIMCP_EXPORT bool dialect_exists(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target
);

/**
 * @brief Get compatibility score between regions
 *
 * WHAT: Measure translation quality
 * WHY:  Assess how well dialects align
 * HOW:  Return learned compatibility metric
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @return Compatibility score (0.0-1.0), or -1.0 if no dialect
 */
NIMCP_EXPORT float dialect_get_compatibility(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target
);

//=============================================================================
// Multi-hop Translation
//=============================================================================

/**
 * @brief Find translation path between regions
 *
 * WHAT: Discover chain of dialects for indirect translation
 * WHY:  Enable communication even without direct dialect
 * HOW:  Breadth-first search through dialect graph
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @param path Output array of region IDs forming path (pre-allocated)
 * @param path_len Output length of path
 * @return 0 on success, negative if no path exists
 *
 * EXAMPLE: If A→B and B→C exist but not A→C:
 * - dialect_get_bridge_path(dl, A, C, path, &len)
 * - Returns path = [A, B, C], len = 3
 * - Can chain: translate(A→B), then translate(B→C)
 */
NIMCP_EXPORT int dialect_get_bridge_path(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target,
    uint32_t* path,
    uint32_t* path_len
);

/**
 * @brief Translate via multi-hop path
 *
 * WHAT: Apply chain of translations
 * WHY:  Enable indirect communication
 * HOW:  Sequential translation through intermediaries
 *
 * @param dl Dialect learner handle
 * @param path Array of region IDs (source to target)
 * @param path_len Length of path
 * @param signal Input signal
 * @param signal_size Signal size
 * @param translated Output translated signal
 * @param translated_size Output size
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_translate_path(
    dialect_learner_t dl,
    const uint32_t* path,
    uint32_t path_len,
    const float* signal,
    uint32_t signal_size,
    float* translated,
    uint32_t* translated_size
);

//=============================================================================
// Dialect Management
//=============================================================================

/**
 * @brief Get all learned dialects
 *
 * WHAT: Retrieve complete dialect catalog
 * WHY:  Inspect all translation mappings
 * HOW:  Return array of all dialects
 *
 * @param dl Dialect learner handle
 * @param dialects Output array of dialects (pre-allocated)
 * @param max_count Maximum dialects to return
 * @param count Output actual number of dialects
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_get_all(
    dialect_learner_t dl,
    neural_dialect_t* dialects,
    uint32_t max_count,
    uint32_t* count
);

/**
 * @brief Remove dialect
 *
 * WHAT: Delete learned translation
 * WHY:  Forget outdated dialect
 * HOW:  Remove from dialect table, free matrix
 *
 * @param dl Dialect learner handle
 * @param source Source region ID
 * @param target Target region ID
 * @return 0 on success, negative if not found
 */
NIMCP_EXPORT int dialect_remove(
    dialect_learner_t dl,
    uint32_t source,
    uint32_t target
);

/**
 * @brief Clear all dialects
 *
 * WHAT: Reset all learned translations
 * WHY:  Start fresh learning
 * HOW:  Remove all dialects, free all matrices
 *
 * @param dl Dialect learner handle
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_clear_all(dialect_learner_t dl);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Dialect learning statistics
 */
typedef struct {
    uint64_t total_dialects_learned;  /**< Total dialects created */
    uint64_t total_translations;      /**< Total translations performed */
    uint64_t total_updates;           /**< Online learning updates */
    uint64_t multihop_translations;   /**< Multi-hop translations */
    float avg_compatibility;          /**< Average dialect compatibility */
    uint32_t active_dialects;         /**< Currently active dialects */
} dialect_learner_stats_t;

/**
 * @brief Get dialect learner statistics
 *
 * WHAT: Retrieve system performance metrics
 * WHY:  Monitor and optimize learning
 * HOW:  Return internal statistics
 *
 * @param dl Dialect learner handle
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_get_stats(
    dialect_learner_t dl,
    dialect_learner_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all statistical counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero internal counters
 *
 * @param dl Dialect learner handle
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_reset_stats(dialect_learner_t dl);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Clone dialect
 *
 * WHAT: Create copy of dialect
 * WHY:  Preserve original while experimenting
 * HOW:  Deep copy of dialect structure
 *
 * @param original Original dialect
 * @param clone Output cloned dialect (pre-allocated)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int dialect_clone(
    const neural_dialect_t* original,
    neural_dialect_t* clone
);

/**
 * @brief Free dialect resources
 *
 * WHAT: Release dialect memory
 * WHY:  Clean up dynamically allocated dialect
 * HOW:  Free translation matrix
 *
 * @param dialect Dialect to free
 */
NIMCP_EXPORT void dialect_free(neural_dialect_t* dialect);

/**
 * @brief Compute dialect similarity
 *
 * WHAT: Measure similarity between two dialects
 * WHY:  Compare translation approaches
 * HOW:  Matrix distance metric
 *
 * @param dialect1 First dialect
 * @param dialect2 Second dialect
 * @return Similarity score (0.0-1.0), or -1.0 on error
 */
NIMCP_EXPORT float dialect_compute_similarity(
    const neural_dialect_t* dialect1,
    const neural_dialect_t* dialect2
);

/**
 * @brief Get last error message
 *
 * WHAT: Retrieve thread-local error string
 * WHY:  Debugging and error reporting
 * HOW:  Return static thread-local buffer
 *
 * @return Error message string
 */
NIMCP_EXPORT const char* dialect_learner_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DIALECT_LEARNING_H */
