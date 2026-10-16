/**
 * @file nimcp_salience.c
 * @brief Salience and attention evaluation implementation
 *
 * WHAT: Fast "interestingness" evaluation for inputs
 * WHY: Active consciousness needs to know what deserves attention
 * HOW: Partial activation + novelty detection + surprise + urgency heuristics
 *
 * PERFORMANCE OPTIMIZATION:
 * - Full decision: ~1ms (full network propagation)
 * - Salience evaluation: ~0.1ms (partial propagation + heuristics)
 * - 10x speedup enables attention-based filtering
 *
 * ALGORITHM:
 * 1. NOVELTY: Compare input to recent history using cosine distance
 * 2. SURPRISE: Compare input to predicted next input (prediction error)
 * 3. URGENCY: Learned patterns + confidence variance
 * 4. SALIENCE: Weighted combination of above
 */

#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/time/nimcp_time.h"
#include "utils/containers/nimcp_vector.h"
#include "utils/logging/nimcp_logging.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Acetylcholine gating

// Bio-async messaging infrastructure
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"  // For error codes

// SNN and Plasticity bridges
#include "cognitive/salience/nimcp_salience_snn_bridge.h"
#include "cognitive/salience/nimcp_salience_plasticity_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

// SIMD intrinsics for vectorized novelty computation
#if defined(__AVX2__)
#include <immintrin.h>
#define SALIENCE_USE_SIMD 1
#define SALIENCE_SIMD_TYPE "AVX2"
#define SALIENCE_SIMD_WIDTH 8
#elif defined(__SSE2__)
#include <immintrin.h>
#define SALIENCE_USE_SIMD 1
#define SALIENCE_SIMD_TYPE "SSE2"
#define SALIENCE_SIMD_WIDTH 4
#else
#define SALIENCE_USE_SIMD 0
#define SALIENCE_SIMD_TYPE "None"
#define SALIENCE_SIMD_WIDTH 1
#endif

#define LOG_MODULE "salience"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(salience, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static _Thread_local char g_salience_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

//=============================================================================
// History Buffer for Novelty Detection (Memento Pattern) - Using nimcp_queue
//=============================================================================

/**
 * WHAT: Maximum feature vector size
 * WHY: Fixed-size entries required for nimcp_queue
 * HOW: Generous limit to handle most use cases
 */
#define SALIENCE_MAX_FEATURES 512

/**
 * WHAT: Single entry in history buffer
 * WHY: Store recent inputs for novelty comparison
 * NOTE: Now uses fixed-size array for nimcp_queue compatibility
 */
typedef struct {
    float features[SALIENCE_MAX_FEATURES];  // Feature vector (fixed-size)
    uint32_t num_features;                  // Actual size used
    uint64_t timestamp;                     // When input occurred
    bool valid;                             // Is this slot occupied?
} history_entry_t;

/**
 * WHAT: Circular buffer for recent input history
 * WHY: Novelty = how different from recent past
 * HOW: Ring buffer with fixed-size entries (no heap allocation per entry)
 *
 * PATTERN: Memento pattern - stores past states for comparison
 * NOTE: Uses fixed-size entries to eliminate per-entry malloc/free
 */
typedef struct {
    history_entry_t* entries;  // Circular buffer (fixed-size entries)
    uint32_t capacity;         // Buffer size
    uint32_t head;             // Next write position
    uint32_t count;            // Current entry count
    nimcp_mutex_t lock;        // Thread safety
} history_buffer_t;

/**
 * WHAT: Create history buffer
 * WHY: Initialize novelty detection storage
 * HOW: Allocate circular buffer
 */

//=============================================================================
// SIMD-Optimized Cosine Distance Computation
//=============================================================================

#if SALIENCE_USE_SIMD

/**
 * WHAT: SIMD-optimized dot product helper
 * WHY: Core operation for cosine similarity - vectorize for performance
 * HOW: Process multiple floats per instruction using SIMD intrinsics
 *
 * PERFORMANCE:
 * - SSE2: 4 floats per instruction (4x speedup potential)
 * - AVX2: 8 floats per instruction (8x speedup potential)
 *
 * @param a First vector
 * @param b Second vector
 * @param n Number of elements
 * @return Dot product sum
 */

#endif  // SALIENCE_USE_SIMD

/**
 * WHAT: Compute novelty score
 * WHY: Novelty = how different from recent history
 * HOW: Compare input to all history entries using cosine distance
 *
 * OPTIMIZATION: Uses SIMD vectorization when available (4-8x speedup)
 *
 * COMPLEXITY: O(h * f) where h = history_size, f = num_features
 *
 * @return Novelty score (0.0 = very familiar, 1.0 = completely novel)
 */

//=============================================================================
// Prediction Model for Surprise Detection
//=============================================================================

/**
 * WHAT: Simple exponential moving average predictor
 * WHY: Predict next input based on recent trend
 * HOW: EMA of recent inputs
 *
 * PATTERN: Memento pattern - maintains prediction state
 */
typedef struct {
    float* prediction;      // Predicted next input
    uint32_t num_features;  // Feature dimension
    float alpha;            // EMA smoothing factor (0-1)
    bool initialized;       // Has seen at least one input?
    nimcp_mutex_t lock;     // Thread safety
} predictor_t;

/**
 * WHAT: Create predictor
 */

//=============================================================================
// Salience Evaluator Structure (Opaque Implementation)
//=============================================================================

/**
 * WHAT: Per-modality salience storage
 * WHY: Track salience separately for each sensory modality before fusion
 * HOW: Store salience scores and activation state per modality
 */
typedef struct {
    brain_salience_t salience;  /**< Most recent salience for this modality */
    float weight;               /**< Fusion weight (normalized) */
    bool active;                /**< Is this modality registered/active? */
    bool has_data;              /**< Has received evaluation data? */
} modality_state_t;

/**
 * WHAT: Complete salience evaluator state
 * WHY: Encapsulates all evaluation data and configuration
 * PATTERN: Opaque pointer (Pimpl idiom)
 */
struct salience_evaluator_struct {
    // Brain association
    brain_t brain;

    // Configuration
    salience_config_t config;

    // Novelty detection (Memento pattern)
    history_buffer_t* history;

    // Surprise detection (Memento pattern)
    predictor_t* predictor;

    // Event callback (Observer pattern)
    salience_event_callback_fn callback;
    void* callback_context;

    // Bio-async module context
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Statistics
    _Atomic uint64_t stats_evaluations;
    _Atomic uint64_t stats_high_salience;
    _Atomic uint64_t stats_high_novelty;
    _Atomic uint64_t stats_high_surprise;
    _Atomic uint64_t stats_high_urgency;

    float running_avg_salience;
    float running_avg_novelty;
    float running_avg_surprise;
    float running_avg_urgency;

    uint64_t total_eval_time_us;

    // Thread safety
    nimcp_mutex_t eval_lock;

    // Parallel processing (for batch operations)
    nimcp_thread_pool_t* thread_pool;

    // Phase 1.5: Memory pool for history entry allocations
    memory_pool_t history_entry_pool;

    // Cross-modal fusion state
    modality_state_t modalities[SALIENCE_MODALITY_COUNT];
    salience_fusion_strategy_t fusion_strategy;

    // SNN and Plasticity bridges
    salience_snn_bridge_t* snn_bridge;             /**< SNN integration bridge */
    salience_plasticity_bridge_t* plasticity_bridge;  /**< Plasticity integration bridge */
    bool bridges_enabled;                          /**< Whether bridges are active */
};

//=============================================================================
// Salience Computation Strategies (Strategy Pattern)
//=============================================================================

/**
 * WHAT: Fast heuristic salience computation
 * WHY: Maximum speed for high-frequency input
 * HOW: Simplified calculations, approximations
 */

//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

/**
 * @brief Broadcast salience evaluation result via bio-async
 */
static void bio_broadcast_salience_response(salience_evaluator_t eval,
                                            const brain_salience_t* salience,
                                            uint32_t stimulus_id);

/**
 * @brief Bio-async message handler: Handle salience query
 *
 * WHAT: Process incoming request to evaluate stimulus salience
 * WHY:  Enable distributed systems to query salience via bio-async
 * HOW:  Create feature vector from query, evaluate salience, send response via promise
 *
 * NOTE: Feature vector is constructed from raw_intensity, novelty, relevance
 */

/**
 * WHAT: Task context for parallel batch evaluation
 * WHY: Worker threads need access to evaluator and specific sample
 * PATTERN: Command pattern - encapsulates evaluation request
 */
typedef struct {
    salience_evaluator_t eval; /* Evaluator to use */
    const float* features;     /* Input features for this sample */
    uint32_t num_features;     /* Feature count */
    brain_salience_t* output;  /* Where to store result */
} batch_task_t;

/**
 * WHAT: Worker function for parallel salience evaluation (stateless)
 * WHY: Executed by thread pool workers
 * HOW: Evaluates single sample WITHOUT updating shared state
 *
 * NOTE: Does NOT update history/predictor to avoid lock contention
 */


// Forward declarations for static functions (SRP split)
static void salience_set_error(const char* format, ...);
static history_buffer_t* history_buffer_create(uint32_t capacity);
static void history_buffer_destroy(history_buffer_t* hist);
static void history_buffer_add(history_buffer_t* hist, const float* features, uint32_t num_features, uint64_t timestamp);
static inline float simd_dot_product(const float* a, const float* b, uint32_t n);
static inline float simd_norm(const float* vec, uint32_t n);
static inline float simd_cosine_distance(const float* a, const float* b, uint32_t n);
static float history_buffer_compute_novelty(history_buffer_t* hist, const float* features, uint32_t num_features);
static void history_buffer_clear(history_buffer_t* hist);
static predictor_t* predictor_create(uint32_t num_features);
static void predictor_destroy(predictor_t* pred);
static void predictor_update(predictor_t* pred, const float* features, uint32_t num_features);
static float predictor_compute_surprise(predictor_t* pred, const float* features, uint32_t num_features);
static brain_salience_t compute_salience_fast(salience_evaluator_t eval, const float* features, uint32_t num_features, uint64_t timestamp);
static brain_salience_t compute_salience_balanced(salience_evaluator_t eval, const float* features, uint32_t num_features, uint64_t timestamp);
static brain_salience_t compute_salience_accurate(salience_evaluator_t eval, const float* features, uint32_t num_features, uint64_t timestamp);
static void apply_acetylcholine_gating(brain_salience_t* salience, brain_t brain);
static bool validate_salience_config(const salience_config_t* config);
static nimcp_error_t handle_salience_query( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static int salience_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data );
static void evaluate_single_task(void* arg);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_salience_part_accessors.c"  // 12 functions: accessors
#include "nimcp_salience_part_lifecycle.c"  // 7 functions: lifecycle
#include "nimcp_salience_part_helpers.c"  // 14 functions: helpers
#include "nimcp_salience_part_processing.c"  // 3 functions: processing
#include "nimcp_salience_part_core.c"  // 15 functions: core
