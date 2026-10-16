/**
 * @file nimcp_self_heal.h
 * @brief Self-Healing Engine - Intelligent Crash Recovery with LNN Integration
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Self-healing engine that combines pattern-based fixes with LNN-learned
 *       crash-to-fix mappings for intelligent automated code repair
 * WHY:  Enable the system to automatically recover from crashes by generating
 *       appropriate fixes, learning from successful repairs over time
 * HOW:  Pattern matching for known crash types, LNN for learning novel fixes,
 *       training on successful outcomes to improve over time
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Innate Immunity (fast)       → Pattern-based fix generation
 * Adaptive Immunity (learned)  → LNN-based fix prediction
 * B Cell Affinity Maturation   → Training on successful fixes
 * Antibody Diversity           → Multiple fix strategies per crash
 * Memory B Cells               → Cached successful crash->fix pairs
 * Clonal Selection             → Best-performing pattern selected
 * Cytokine Signaling           → Fix propagation to related modules
 * Immunological Memory         → Persistent learning of new patterns
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        SELF-HEALING ENGINE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    CRASH ANALYSIS                                   │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐    │  ║
 * ║   │   │  Antigen     │  │  Crash       │  │  Feature             │    │  ║
 * ║   │   │  Input       │──│  Signature   │──│  Extraction          │    │  ║
 * ║   │   │              │  │  Analysis    │  │  (for LNN)           │    │  ║
 * ║   │   └──────────────┘  └──────────────┘  └──────────────────────┘    │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    FIX GENERATION                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────────────┐    ┌─────────────────────────────┐   │  ║
 * ║   │   │    PATTERN-BASED        │    │    LNN-BASED                │   │  ║
 * ║   │   │  ─────────────────────  │    │  ───────────────────────    │   │  ║
 * ║   │   │  - NULL check           │    │  - Feature input            │   │  ║
 * ║   │   │  - Bounds check         │    │  - LNN forward pass         │   │  ║
 * ║   │   │  - Div-zero check       │    │  - Fix type prediction      │   │  ║
 * ║   │   │  - UAF protection       │    │  - Confidence score         │   │  ║
 * ║   │   │  - Alignment fix        │    │  - Template selection       │   │  ║
 * ║   │   └─────────────┬───────────┘    └─────────────┬───────────────┘   │  ║
 * ║   │                 │                              │                   │  ║
 * ║   │                 └──────────────┬───────────────┘                   │  ║
 * ║   │                                ▼                                   │  ║
 * ║   │                   ┌─────────────────────────┐                      │  ║
 * ║   │                   │     FIX CANDIDATE       │                      │  ║
 * ║   │                   │     SELECTION           │                      │  ║
 * ║   │                   └─────────────────────────┘                      │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    LEARNING & FEEDBACK                              │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐    │  ║
 * ║   │   │  Success/    │  │  LNN         │  │  Pattern Stats       │    │  ║
 * ║   │   │  Failure     │──│  Training    │──│  Update              │    │  ║
 * ║   │   │  Feedback    │  │  Update      │  │                      │    │  ║
 * ║   │   └──────────────┘  └──────────────┘  └──────────────────────┘    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SELF_HEAL_H
#define NIMCP_SELF_HEAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_heal_patterns.h"
#include "lnn/nimcp_lnn.h"

/* Forward declaration for signal handler types */
struct signal_crash_context;
typedef struct signal_crash_context signal_crash_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SELF_HEAL_MAX_CODE_SIZE          4096    /**< Max code snippet size */
#define SELF_HEAL_MAX_FIX_CANDIDATES     8       /**< Max fix candidates per crash */
#define SELF_HEAL_FEATURE_DIM            64      /**< LNN feature vector dimension */
#define SELF_HEAL_LNN_HIDDEN_SIZE        32      /**< LNN hidden layer size */
#define SELF_HEAL_DEFAULT_CONFIDENCE     0.5f    /**< Default fix confidence */
#define SELF_HEAL_MODULE_NAME            "self_heal"

/* LNN training parameters */
#define SELF_HEAL_LNN_LEARNING_RATE      0.001f  /**< LNN learning rate */
#define SELF_HEAL_LNN_DT                 1.0f    /**< LNN time step */
#define SELF_HEAL_TRAINING_BATCH_SIZE    16      /**< Training batch size */
#define SELF_HEAL_MAX_TRAINING_SAMPLES   1024    /**< Max training samples */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct self_heal_engine_s self_heal_engine_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Self-healing engine mode
 *
 * WHAT: Operating mode of the engine
 * WHY:  Different modes for different use cases
 */
typedef enum {
    HEAL_MODE_PATTERN_ONLY = 0,    /**< Use only pattern matching (fast) */
    HEAL_MODE_LNN_ONLY,            /**< Use only LNN prediction (flexible) */
    HEAL_MODE_HYBRID,              /**< Combine pattern + LNN (default) */
    HEAL_MODE_LEARNING             /**< Learning mode with training */
} self_heal_mode_t;

/**
 * @brief Fix generation status
 *
 * WHAT: Outcome of fix generation attempt
 * WHY:  Track success/failure reasons
 */
typedef enum {
    HEAL_STATUS_SUCCESS = 0,       /**< Fix generated successfully */
    HEAL_STATUS_NO_PATTERN,        /**< No matching pattern found */
    HEAL_STATUS_LNN_FAILURE,       /**< LNN prediction failed */
    HEAL_STATUS_LOW_CONFIDENCE,    /**< Confidence below threshold */
    HEAL_STATUS_CODE_TOO_LARGE,    /**< Source code exceeds limits */
    HEAL_STATUS_INVALID_INPUT,     /**< Invalid input parameters */
    HEAL_STATUS_INTERNAL_ERROR     /**< Internal engine error */
} heal_status_t;

/**
 * @brief Crash feature type for LNN encoding
 *
 * WHAT: Types of features extracted from crashes
 * WHY:  Structure feature vector for LNN input
 */
typedef enum {
    FEATURE_CRASH_TYPE = 0,        /**< Type of crash (SIGSEGV, etc.) */
    FEATURE_SOURCE_PATTERN,        /**< Source code pattern hash */
    FEATURE_CONTEXT,               /**< Surrounding code context */
    FEATURE_VARIABLE_TYPE,         /**< Variable type involved */
    FEATURE_STACK_DEPTH,           /**< Stack trace depth */
    FEATURE_FREQUENCY              /**< Crash frequency */
} crash_feature_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Crash feature vector for LNN input
 *
 * WHAT: Numerical representation of crash for LNN processing
 * WHY:  LNN requires fixed-dimension numerical input
 */
typedef struct {
    float features[SELF_HEAL_FEATURE_DIM]; /**< Feature values */
    size_t n_features;                      /**< Number of valid features */
    fix_pattern_type_t suggested_type;      /**< Pre-analyzed pattern type */
    float type_confidence;                  /**< Confidence in suggested type */
} crash_features_t;

/**
 * @brief Healing result from fix generation
 *
 * WHAT: Complete result of fix generation attempt
 * WHY:  Contains fixed code and metadata
 */
typedef struct {
    char original_code[SELF_HEAL_MAX_CODE_SIZE]; /**< Original source code */
    char fixed_code[SELF_HEAL_MAX_CODE_SIZE];    /**< Generated fix */
    fix_pattern_type_t pattern_used;             /**< Pattern type applied */
    float confidence;                            /**< Fix confidence (0-1) */
    bool lnn_generated;                          /**< True if LNN contributed */
    heal_status_t status;                        /**< Generation status */
    uint32_t pattern_id;                         /**< Pattern ID used */
    uint64_t generation_time_us;                 /**< Generation time in microseconds */
} heal_result_t;

/**
 * @brief Fix candidate for ranking
 *
 * WHAT: Potential fix with associated score
 * WHY:  Compare multiple fix options
 */
typedef struct {
    heal_result_t result;                        /**< Fix result */
    float score;                                 /**< Ranking score */
    bool pattern_based;                          /**< True if from pattern */
    bool lnn_based;                              /**< True if from LNN */
} fix_candidate_t;

/**
 * @brief Training sample for LNN
 *
 * WHAT: Crash->fix pair for supervised learning
 * WHY:  Train LNN on successful fixes
 */
typedef struct {
    crash_features_t features;                   /**< Input features */
    fix_pattern_type_t correct_fix_type;         /**< Ground truth fix type */
    float success_score;                         /**< How well fix worked */
    uint64_t timestamp;                          /**< When sample was recorded */
} training_sample_t;

/**
 * @brief Self-healing engine configuration
 *
 * WHAT: Configuration parameters for self-healing engine
 * WHY:  Customize engine behavior
 */
typedef struct {
    self_heal_mode_t mode;                       /**< Operating mode */
    float confidence_threshold;                  /**< Minimum confidence for fixes */
    bool enable_lnn;                             /**< Enable LNN integration */
    bool enable_learning;                        /**< Enable online learning */
    bool enable_logging;                         /**< Enable debug logging */

    /* LNN configuration */
    uint32_t lnn_hidden_size;                    /**< LNN hidden layer size */
    float lnn_learning_rate;                     /**< LNN learning rate */
    uint32_t max_training_samples;               /**< Max samples to retain */

    /* Pattern configuration */
    bool use_custom_patterns;                    /**< Allow custom patterns */
    uint32_t max_custom_patterns;                /**< Max custom patterns */

    /* Integration */
    brain_immune_system_t* immune_system;        /**< Optional immune system link */
} self_heal_config_t;

/**
 * @brief Self-healing engine statistics
 *
 * WHAT: Runtime statistics for monitoring
 * WHY:  Track engine performance
 */
typedef struct {
    uint64_t crashes_analyzed;                   /**< Total crashes analyzed */
    uint64_t fixes_generated;                    /**< Successful fix generations */
    uint64_t pattern_fixes;                      /**< Fixes from patterns */
    uint64_t lnn_fixes;                          /**< Fixes from LNN */
    uint64_t hybrid_fixes;                       /**< Fixes from pattern+LNN */
    uint64_t failed_fixes;                       /**< Failed fix attempts */

    /* Training stats */
    uint64_t training_samples;                   /**< Training samples collected */
    uint64_t training_updates;                   /**< LNN training updates */
    float avg_confidence;                        /**< Average fix confidence */
    float success_rate;                          /**< Verified success rate */

    /* Performance */
    double avg_generation_time_ms;               /**< Average fix generation time */
    size_t memory_usage_bytes;                   /**< Current memory usage */
} self_heal_stats_t;

/**
 * @brief Self-healing engine state (opaque)
 *
 * WHAT: Internal engine state
 * WHY:  Encapsulate implementation details
 */
struct self_heal_engine_s {
    self_heal_config_t config;                   /**< Configuration */
    pattern_library_t* pattern_library;          /**< Pattern library */

    /* LNN integration */
    lnn_network_t* lnn_network;                  /**< LNN for fix prediction */
    lnn_training_ctx_t* lnn_training;            /**< LNN training context */
    bool lnn_initialized;                        /**< LNN ready flag */

    /* Training data */
    training_sample_t* training_samples;         /**< Training sample buffer */
    size_t n_training_samples;                   /**< Current sample count */
    size_t training_sample_capacity;             /**< Sample buffer capacity */

    /* Immune system integration */
    brain_immune_system_t* immune_system;        /**< Linked immune system */
    bool immune_connected;                       /**< Immune system connected */

    /* Statistics */
    self_heal_stats_t stats;                     /**< Runtime statistics */

    /* Thread safety */
    void* mutex;                                 /**< Access mutex */

    /* State */
    bool initialized;                            /**< Engine initialized */
    uint64_t start_time;                         /**< Engine start time */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Easy initialization
 * HOW:  Set hybrid mode, enable LNN, default thresholds
 *
 * @param config Configuration to populate
 * @return 0 on success, -1 on error
 */
int self_heal_default_config(self_heal_config_t* config);

/**
 * @brief Create self-healing engine
 *
 * WHAT: Initialize self-healing engine
 * WHY:  Set up pattern library and optional LNN
 * HOW:  Allocate resources, initialize LNN if enabled
 *
 * @param config Configuration (NULL for defaults)
 * @return Engine handle or NULL on failure
 */
self_heal_engine_t* self_heal_create(const self_heal_config_t* config);

/**
 * @brief Destroy self-healing engine
 *
 * WHAT: Clean up engine resources
 * WHY:  Proper resource deallocation
 * HOW:  Free LNN, patterns, training data
 *
 * @param engine Engine to destroy (NULL-safe)
 */
void self_heal_destroy(self_heal_engine_t* engine);

/* ============================================================================
 * Crash Analysis API
 * ============================================================================ */

/**
 * @brief Analyze crash and determine fix pattern type
 *
 * WHAT: Analyze crash signature to identify fix type
 * WHY:  First step in fix generation
 * HOW:  Extract features, match patterns, optionally use LNN
 *
 * @param engine Self-healing engine
 * @param antigen Crash antigen from immune system
 * @return Suggested fix pattern type
 */
fix_pattern_type_t self_heal_analyze_crash(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen
);

/**
 * @brief Extract features from crash for LNN
 *
 * WHAT: Convert crash to numerical feature vector
 * WHY:  LNN requires numerical input
 * HOW:  Encode antigen properties to feature vector
 *
 * @param engine Self-healing engine
 * @param antigen Crash antigen
 * @param features Output feature vector
 * @return 0 on success, negative on error
 */
int self_heal_extract_features(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    crash_features_t* features
);

/**
 * @brief Extract extended features from crash context
 *
 * WHAT: Convert crash context (signal, backtrace) to numerical features
 * WHY:  More detailed crash signature for better fix prediction
 * HOW:  Encode signal type, addresses, backtrace patterns
 *
 * @param engine Self-healing engine
 * @param crash_ctx Crash context from signal handler
 * @param features Output feature vector
 * @return 0 on success, negative on error
 */
int self_heal_extract_features_from_context(
    self_heal_engine_t* engine,
    const signal_crash_context_t* crash_ctx,
    crash_features_t* features
);

/* ============================================================================
 * Fix Generation API
 * ============================================================================ */

/**
 * @brief Generate fix for crash
 *
 * WHAT: Generate code fix for given crash
 * WHY:  Main fix generation entry point
 * HOW:  Analyze crash, match pattern or use LNN, generate fix code
 *
 * @param engine Self-healing engine
 * @param antigen Crash antigen
 * @param source_code Original source code at crash site
 * @param result Output: healing result with fixed code
 * @return 0 on success, negative on error
 */
int self_heal_generate_fix(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const char* source_code,
    heal_result_t* result
);

/**
 * @brief Generate multiple fix candidates
 *
 * WHAT: Generate ranked list of potential fixes
 * WHY:  Allow selection of best fix
 * HOW:  Generate from patterns and LNN, rank by confidence
 *
 * @param engine Self-healing engine
 * @param antigen Crash antigen
 * @param source_code Original source code
 * @param candidates Output: array of fix candidates
 * @param max_candidates Maximum candidates to generate
 * @return Number of candidates generated
 */
int self_heal_generate_candidates(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const char* source_code,
    fix_candidate_t* candidates,
    size_t max_candidates
);

/**
 * @brief Use LNN to predict fix type
 *
 * WHAT: Use LNN to predict best fix pattern type
 * WHY:  Handle novel crashes not matching patterns
 * HOW:  Forward pass through LNN with crash features
 *
 * @param engine Self-healing engine
 * @param features Crash feature vector
 * @param fix_type_out Output: predicted fix type
 * @return Prediction confidence (0-1)
 */
float self_heal_lnn_predict(
    self_heal_engine_t* engine,
    const crash_features_t* features,
    fix_pattern_type_t* fix_type_out
);

/* ============================================================================
 * Learning API
 * ============================================================================ */

/**
 * @brief Train on successful fix
 *
 * WHAT: Update LNN with successful fix
 * WHY:  Learn from successful repairs
 * HOW:  Add to training set, run training step
 *
 * @param engine Self-healing engine
 * @param antigen Original crash antigen
 * @param fix Applied fix result
 * @return 0 on success, negative on error
 */
int self_heal_train_on_success(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const heal_result_t* fix
);

/**
 * @brief Train on failed fix
 *
 * WHAT: Update LNN to avoid failed fix type
 * WHY:  Learn from failures
 * HOW:  Add negative sample, update LNN
 *
 * @param engine Self-healing engine
 * @param antigen Original crash antigen
 * @param fix Failed fix result
 * @return 0 on success, negative on error
 */
int self_heal_train_on_failure(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen,
    const heal_result_t* fix
);

/**
 * @brief Run batch training update
 *
 * WHAT: Run training on accumulated samples
 * WHY:  Batch training more efficient
 * HOW:  Forward/backward pass on training batch
 *
 * @param engine Self-healing engine
 * @return 0 on success, negative on error
 */
int self_heal_train_batch(self_heal_engine_t* engine);

/**
 * @brief Perform single-step online learning update
 *
 * WHAT: Update LNN weights immediately after fix attempt
 * WHY:  Faster adaptation to new crash patterns
 * HOW:  Single forward/backward pass with current sample
 *
 * @param engine Self-healing engine
 * @param features Crash feature vector
 * @param correct_type Ground truth fix type
 * @param success_score How well the fix worked (0-1)
 * @return 0 on success, negative on error
 */
int self_heal_train_online(
    self_heal_engine_t* engine,
    const crash_features_t* features,
    fix_pattern_type_t correct_type,
    float success_score
);

/**
 * @brief Apply temporal decay to training samples
 *
 * WHAT: Decay old sample weights to prioritize recent data
 * WHY:  Online learning should favor recent patterns
 * HOW:  Apply exponential decay based on sample age
 *
 * @param engine Self-healing engine
 * @return 0 on success, negative on error
 */
int self_heal_decay_samples(self_heal_engine_t* engine);

/**
 * @brief Save LNN model to file
 *
 * WHAT: Persist trained LNN to disk
 * WHY:  Preserve learned knowledge
 * HOW:  Serialize LNN weights to file
 *
 * @param engine Self-healing engine
 * @param path File path to save
 * @return 0 on success, negative on error
 */
int self_heal_save_model(
    self_heal_engine_t* engine,
    const char* path
);

/**
 * @brief Load LNN model from file
 *
 * WHAT: Restore trained LNN from disk
 * WHY:  Reuse previously learned model
 * HOW:  Deserialize LNN weights from file
 *
 * @param engine Self-healing engine
 * @param path File path to load
 * @return 0 on success, negative on error
 */
int self_heal_load_model(
    self_heal_engine_t* engine,
    const char* path
);

/* ============================================================================
 * Pattern Management API
 * ============================================================================ */

/**
 * @brief Get pattern by type
 *
 * WHAT: Retrieve fix pattern definition
 * WHY:  Access pattern for manual inspection
 * HOW:  Look up in pattern library
 *
 * @param engine Self-healing engine
 * @param type Pattern type
 * @return Pattern definition or NULL if not found
 */
const fix_pattern_t* self_heal_get_pattern(
    self_heal_engine_t* engine,
    fix_pattern_type_t type
);

/**
 * @brief Register custom pattern
 *
 * WHAT: Add user-defined fix pattern
 * WHY:  Extend with domain-specific patterns
 * HOW:  Add to pattern library
 *
 * @param engine Self-healing engine
 * @param pattern Pattern to register
 * @return 0 on success, negative on error
 */
int self_heal_register_pattern(
    self_heal_engine_t* engine,
    const fix_pattern_t* pattern
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to immune system
 *
 * WHAT: Link engine to brain immune system
 * WHY:  Receive crash antigens automatically
 * HOW:  Register as antigen callback
 *
 * @param engine Self-healing engine
 * @param immune_system Brain immune system
 * @return 0 on success, negative on error
 */
int self_heal_connect_immune(
    self_heal_engine_t* engine,
    brain_immune_system_t* immune_system
);

/**
 * @brief Handle antigen from immune system
 *
 * WHAT: Process crash antigen from immune callback
 * WHY:  Automatic crash handling
 * HOW:  Analyze and attempt fix generation
 *
 * @param engine Self-healing engine
 * @param antigen Crash antigen
 * @return 0 on success, negative on error
 */
int self_heal_handle_antigen(
    self_heal_engine_t* engine,
    const brain_antigen_t* antigen
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get engine statistics
 *
 * WHAT: Retrieve runtime statistics
 * WHY:  Monitor engine performance
 * HOW:  Copy statistics structure
 *
 * @param engine Self-healing engine
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int self_heal_get_stats(
    self_heal_engine_t* engine,
    self_heal_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero statistics structure
 *
 * @param engine Self-healing engine
 * @return 0 on success, negative on error
 */
int self_heal_reset_stats(self_heal_engine_t* engine);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get healing status string
 *
 * @param status Healing status
 * @return String description
 */
const char* self_heal_status_to_string(heal_status_t status);

/**
 * @brief Get healing mode string
 *
 * @param mode Healing mode
 * @return String description
 */
const char* self_heal_mode_to_string(self_heal_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_HEAL_H */
