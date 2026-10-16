/**
 * @file nimcp.c
 * @brief Implementation of unified NIMCP API
 *
 * This file wraps the internal APIs (brain, neural_network, ethics, knowledge)
 * and provides a consistent, stable public interface.
 */

#include "async/nimcp_bio_async.h"
#include "constants/nimcp_buffer_constants.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "API"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nimcp)

/* Exception integration for API layer */
static void api_set_error(const char* fmt, ...);
#define NIMCP_API_SET_ERROR(fmt, ...) api_set_error(fmt, ##__VA_ARGS__)
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"  /* For exception system shutdown */

#include "nimcp.h"
#include "security/nimcp_audit_log.h"
#include "api/nimcp_api_internal.h"  /* Canonical handle struct definitions */
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_cognitive_transcript.h"
#include "core/brain/strategy/nimcp_brain_strategy.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "core/neuralnet/nimcp_neuralnet_backprop.h"  // Backprop for gradient-based training
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/nimcp_working_memory.h"  // Phase 10.2: Working Memory API
#include "cognitive/global_workspace/nimcp_global_workspace.h"  // Global Workspace Architecture
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"  // Multi-brain collective
#include "utils/memory/nimcp_memory.h"
#include "utils/config/nimcp_config.h"
#include "utils/cache/nimcp_cache.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_blood_brain_barrier.h"  // Phase IS-1: BBB perimeter defense
#include "security/nimcp_constant_time.h"        // Constant-time crypto operations
#include "core/brain/nimcp_brain_internal.h"     // For accessing brain->bbb_system
#include "language/nimcp_language_orchestrator.h" // For nimcp_brain_speak
#include "language/nimcp_language_types.h"        // LANGUAGE_OUTPUT_TEXT
#include "generation/nimcp_language_generator.h"  // LNN-based text generation
#include "generation/nimcp_tokenizer.h"           // Tokenizer encode/decode
#include "generation/nimcp_embedding.h"           // Token embeddings
#include "language/nimcp_grounded_language.h"     // Grounded language system
#include "language/nimcp_bigram_spectrum.h"        // PA-4+ bigram FFT diagnostics
#include "snn/bridges/nimcp_snn_language_bridge.h" // SNN-language spike bridge (diagnostics)
#include "cognitive/nimcp_emotional_system.h"     // For avatar emotional state
#include "middleware/training/nimcp_brain_training_integration.h"  // Training coordinator
#include "middleware/training/nimcp_loss_functions.h"              // Loss functions
#include "middleware/training/nimcp_optimizers.h"                  // Optimizers
#include "middleware/training/nimcp_lr_scheduler.h"                // LR schedulers
#include "middleware/training/nimcp_training_callbacks.h"          // Training callbacks
#include "training/nimcp_training_dispatch.h"                      // SNN/LNN/CNN/Adaptive dispatch
#include "plasticity/adaptive/nimcp_adaptive.h"                    // Adaptive network
#include "core/neuralnet/nimcp_neuralnet_learning.h"              // neural_network_apply_reward_learning_active
#include "core/brain/learning/nimcp_brain_learning.h"              // brain_learn_vector, brain_learn_example
#include "cognitive/rubric/nimcp_rubric.h"                         // Cognitive output rubric
#include "middleware/cloud/nimcp_cloud_inference.h"                 // Edge-cloud hybrid inference
#include "utils/platform/nimcp_platform_once.h"                    // Thread-safe init
#include "utils/thread/nimcp_atomic.h"                             // Atomic operations
#include "utils/exception/nimcp_exception_macros.h"
#include "core/brain/inference/nimcp_brain_inference.h"  // brain_freeze, brain_is_frozen
#include "core/brain/factory/nimcp_brain_factory.h"      // nimcp_brain_factory_init_brain_config
#include <stdio.h>
#include <stdlib.h>   /* strtod for base-lexicon JSON parser */
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>   /* SIGPIPE ignore in nimcp_init_internal */
#include <float.h>  // I-H3 FIX: FLT_MAX for NaN-safe argmax in predict_fast

/**
 * @brief API-layer check+throw that also sets the public error string
 *
 * NIMCP_CHECK_THROW only throws exceptions but doesn't update g_last_error.
 * This macro ensures nimcp_get_error() returns the right message for API callers.
 */
#define API_CHECK_THROW(cond, code, msg) \
    do { \
        if (!(cond)) { \
            set_error("%s", (msg)); \
            NIMCP_THROW((code), "%s", (msg)); \
            return (code); \
        } \
    } while (0)

/* Handle structures are defined in api/nimcp_api_internal.h (canonical source) */

//=============================================================================
// Global State
//=============================================================================

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = "No error";
static nimcp_atomic_bool_t g_initialized = {0};  // Thread-safe initialized flag
static nimcp_atomic_bool_t g_init_in_progress = {0};  // Guard against concurrent init
// P2 FIX: g_init_result must be atomic since it's read by spinning threads
// without synchronization. Without _Atomic, a thread spinning in nimcp_init()
// could read a torn/stale value.
static _Atomic nimcp_status_t g_init_result = NIMCP_OK;  // Result of init

// Forward declaration: defined later in this file after static variables
extern void nimcp_api_reset_brain_probe_ctx(void);


// Forward declaration for training cleanup (defined in nimcp_api_training.c)
extern void nimcp_api_training_cleanup_brain(nimcp_brain_t brain);


/**
 * @brief Module context for brain bio-router registration (lazy init)
 */
static bio_module_context_t g_brain_probe_module_ctx = NULL;

// P1-8 FIX: Mutex for thread-safe lazy initialization of g_brain_probe_module_ctx
static nimcp_platform_once_t g_brain_probe_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// Complex Number & Oscillation API Implementation
//=============================================================================

#include "core/brain/oscillations/nimcp_brain_complex_oscillations.h"
#include "security/nimcp_bbb_helpers.h"

/**
 * @brief Enable or disable complex oscillation features
 */

//=============================================================================
// Training Pipeline API Implementation
//=============================================================================

/**
 * @brief Internal structure to track training configuration IDs
 *
 * Stored in brain handle to track created loss/optimizer/scheduler IDs
 */
typedef struct {
    uint32_t loss_id;
    uint32_t optimizer_id;
    uint32_t scheduler_id;
    uint32_t gradmgr_id;
    bool configured;
    uint32_t step_count;
    tcb_context_t* callbacks;         /**< Training callback manager */
    bool callbacks_enabled;           /**< Whether to fire callbacks */
    backprop_ctx_t* backprop;         /**< Backpropagation context for weight gradients */

    /* Rubric tracking */
    bool rubric_enabled;
    uint32_t rubric_interval;
    float rubric_min_score;
    bool rubric_stop_on_threshold;
    float* rubric_validation_features;       /**< User-provided validation features (owned) */
    uint32_t rubric_validation_num_features;
    uint64_t rubric_eval_count;
    double rubric_score_sum;
    float rubric_min_observed;
    float rubric_max_observed;
    nimcp_rubric_t rubric_last;
} training_pipeline_state_t;

// Global map from brain handle to training state (simple approach for now)
// In production, this would be stored in the brain handle struct
/* P1-46: Add mutex for thread-safe access to g_training_states */
#define MAX_TRAINING_STATES 64
static nimcp_mutex_t g_training_states_mutex = NIMCP_MUTEX_INITIALIZER;
static struct {
    nimcp_brain_t brain;
    training_pipeline_state_t state;
} g_training_states[MAX_TRAINING_STATES] = {0};


//=============================================================================
// Training Callbacks API Implementation
//=============================================================================

/**
 * @brief Wrapper to convert public callback to internal callback
 */
typedef struct {
    nimcp_training_callback_fn public_callback;
    void* user_data;
} callback_wrapper_t;

/**
 * @brief Internal callback that bridges public API to internal API
 */

// Track callback wrappers for cleanup
#define MAX_CALLBACK_WRAPPERS 256
static callback_wrapper_t* g_callback_wrappers[MAX_CALLBACK_WRAPPERS] = {0};
static uint32_t g_next_wrapper_id = 0;
static nimcp_mutex_t g_callback_wrappers_mutex = NIMCP_MUTEX_INITIALIZER;



// Forward declarations for static functions (SRP split)
static void set_error(const char* fmt, ...);
static nimcp_status_t nimcp_init_internal(void);
static void brain_probe_module_init_once(void);
static bio_module_context_t get_brain_probe_module_ctx(void);
static cognitive_module_t convert_module_enum(nimcp_cognitive_module_t public_module);
static training_pipeline_state_t* get_training_state(nimcp_brain_t brain);
static void clear_training_state(nimcp_brain_t brain);
static tcb_action_t callback_bridge(const tcb_event_t* event);
static tcb_action_t fire_training_callback( training_pipeline_state_t* state, tcb_event_type_t event_type, float loss, float learning_rate, uint64_t step, float gradient_norm);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_part_core.c"  // 32 functions: core
#include "nimcp_part_helpers.c"  // 7 functions: helpers
#include "nimcp_part_accessors.c"  // 14 functions: accessors
#include "nimcp_part_lifecycle.c"  // 16 functions: lifecycle
#include "nimcp_part_io.c"  // 4 functions: io
#include "nimcp_part_stats.c"  // 2 functions: stats
#include "nimcp_part_processing.c"  // 2 functions: processing
#include "nimcp_part_bindings.c"  // 20 functions: binding-language public API wrappers
#include "nimcp_part_alloc_stats.c"  // 1 function: nimcp_get_alloc_stats — leak attribution
