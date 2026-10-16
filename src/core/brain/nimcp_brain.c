//=============================================================================
// nimcp_brain.c - High-Level Brain API Implementation (Refactored)
//=============================================================================
/**
 * @file nimcp_brain.c
 * @brief Production-ready brain API with Factory and Strategy patterns
 *
 * ARCHITECTURE:
 * - Factory Pattern: Creates brains of different types with validated configs
 * - Strategy Pattern: Task-specific behaviors (classification, regression, etc.)
 * - Builder Pattern: Modular configuration construction
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Brain creation: O(n) where n = num_neurons
 * - Learning: O(s*n) where s = sparsity, n = active_neurons
 * - Inference: O(s*n) with caching for repeated inputs
 * - Memory: O(n*c) where c = average_connections_per_neuron
 *
 * DESIGN DECISIONS:
 * - No nested ifs: All validation uses early returns (guard clauses)
 * - Functions <50 lines: Complex operations decomposed into helpers
 * - Caching: Decision results cached for repeated identical inputs
 * - Thread-safe: Error handling uses thread-local storage
 */

#include "core/brain/nimcp_brain.h"
#include "security/nimcp_audit_log.h"
#include "security/lgss/nimcp_lgss.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/nimcp_brain_multimodal.h"  // Extracted multimodal processing
#include "utils/memory/nimcp_unified_memory.h"
#include "core/brain/nimcp_brain_internal.h"
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "snn/bridges/nimcp_snn_visual_bridge.h"
#include "snn/bridges/nimcp_snn_audio_bridge.h"
#include "snn/bridges/nimcp_snn_speech_bridge.h"
#include "snn/bridges/nimcp_snn_somatosensory_bridge.h"
#include "snn/bridges/nimcp_snn_cross_modal_align.h"
#include "core/brain/bridges/nimcp_hyperledger_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "core/brain/nimcp_brain_lazy_init.h"
#include "core/probes/nimcp_probe_stages.h"

#define LOG_MODULE "BRAIN"
#include <math.h>
#include <float.h>  // W7: FLT_MAX for NaN-safe argmax in determine_output_label
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/cache/nimcp_cache.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"  // Thread-safe one-time initialization
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"  // Phase 11: Biological attack defense
#include "async/nimcp_bio_async.h"    // Bio-async messaging system
#include "async/nimcp_bio_router.h"   // Bio-async message router
#include "async/nimcp_bio_messages.h" // Bio-async message types
#include "core/brain/nimcp_brain_bio_async.h" // Brain bio-async integration API
#include "utils/memory/nimcp_memory_guards.h" // For nimcp_calloc/nimcp_free

/* Logging module identifier (undef to suppress redefinition warning) */
#undef LOG_MODULE
#define LOG_MODULE "BRAIN"
#include "core/topology/nimcp_community_detection.h"  // Community detection & topology analysis
#include "utils/algorithms/nimcp_graph_metrics.h"       // Graph metrics
#include "utils/containers/nimcp_graph.h"                // Graph data structures

// Comprehensive Integration: All Advanced Subsystems
// NOTE: Only including modules that currently exist
#include "glial/integration/nimcp_glial_integration.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"       // Myelin sheath structural modeling
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"  // G1/G2: glial destroy + attach helpers
#include "core/axon/nimcp_axon.h"                          // Phase 1.5.6: Axon signal propagation
#include "core/dendrite/nimcp_dendrite.h"                  // Phase 1.5.7: Dendrite integration
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/nimcp_symbolic_logic.h"              // Phase 9.4: Symbolic reasoning
#include "cognitive/epistemic/nimcp_epistemic_filter.h"  // Phase 9.2: Bias prevention
#include "cognitive/wellbeing/nimcp_wellbeing.h"        // Phase 9.3: Self-preservation
#include "cognitive/omni/bridges/nimcp_omni_wm_thousand_brains_bridge.h"  // Hawkins TB WM bridge
#include "core/cortical_columns/nimcp_thousand_brains_integration.h"      // TB full-system integration hub
#include "core/cortical_columns/nimcp_cortical_column.h"                  // Cortical columns base API
#include "core/cortical_columns/nimcp_cortical_column_ternary.h"          // Ternary WTA wrapper
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"   // Full neuromodulator system
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"  // Phase C2.1: Spatial neuromodulator diffusion
#include "plasticity/attention/nimcp_attention.h"               // Multihead attention mechanism
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h" // Attention-plasticity bridge for training
#include "core/neuron_types/nimcp_neural_logic.h"
#include "cognitive/nimcp_fractal_cognitive.h"                  // NIMCP 2.7 Phase 8.5: Fractal topology cognitive integration
// Forward declarations for rubric evaluator (avoids including rubric.h which
// has brain_decision_t forward decl conflicting with the anonymous struct definition)
struct rubric_evaluator;
void rubric_evaluator_destroy(struct rubric_evaluator* eval);

// Phase 8: Multi-Modal Integration
#include "core/integration/nimcp_multimodal_integration.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "nlp/nimcp_nlp.h"                      // Natural language processing

// Brain Regions Architecture (hierarchical cortical organization)
#include "core/brain_regions/nimcp_brain_regions.h"

// Phase 10: Advanced Cognitive Architecture
#include "cognitive/nimcp_working_memory.h"    // Phase 10.1: Miller's 7±2 working memory
#include "cognitive/nimcp_emotional_tagging.h" // Phase 10.2: Emotional tagging (Russell's circumplex)
#include "cognitive/nimcp_emotional_system.h"  // Phase 10.2: Integrated emotional system
#include "cognitive/nimcp_executive.h"         // Phase 10.3: Executive Functions (task switching, planning)
#include "cognitive/nimcp_sleep_wake.h"        // Phase 10.4: Sleep/wake cycle (consolidation, homeostasis)
#include "cognitive/memory/nimcp_engram.h"     // Phase M1: Memory Engrams (distributed memory traces)
#include "cognitive/memory/nimcp_systems_consolidation.h" // Phase M2: Systems Consolidation (hippocampus → cortex)
#include "cognitive/memory/nimcp_wm_transfer.h" // Phase M3: Working Memory Transfer (WM → engram encoding)
#include "cognitive/memory/nimcp_semantic_memory.h" // Phase M4: Semantic Memory Network (concept network + spreading activation)
#include "cognitive/nimcp_mental_health.h"     // Phase 10.5: Mental Health Monitoring (disorder detection)
#include "cognitive/nimcp_theory_of_mind.h"    // Phase 10.6: Theory of Mind (BDI model, empathy)
#include "cognitive/nimcp_explanations.h"      // Phase 10.7: Natural Explanations (interpretability)
#include "cognitive/nimcp_meta_learning.h"     // Phase 10.8: Meta-Learning (MAML, few-shot learning)
#include "cognitive/nimcp_predictive.h"        // Phase 10.9: Predictive Processing (free energy minimization)
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"  // Predictive hierarchy (C5/C6 inference)
#include "cognitive/jepa/nimcp_jepa_latent.h"                 // JEPA latent representations (C5/C6 inference)
#include "cognitive/jepa/nimcp_jepa_predictor.h"              // JEPA predictor training (C6 inference)
#include "language/nimcp_grounded_language.h"                 // Grounded language (C6 inference training)
#include "cognitive/immune/nimcp_self_heal.h"                 // Self-heal engine (C6 inference training)
#include "cognitive/free_energy/nimcp_free_energy.h"          // FEP orchestrator (C6 inference training)
#include "cognitive/vae/bridges/nimcp_vae_training_bridge.h"  // VAE training bridge (C6 inference training)
#include "cognitive/nimcp_mirror_neurons.h"    // Phase 10.11: Mirror Neurons (social cognition, imitation)
#include "cognitive/global_workspace/nimcp_global_workspace.h"  // Global Workspace Architecture (GWT)
#include "cognitive/nimcp_autobiographical_memory.h"  // Phase 12: Autobiographical Memory (episodic self-memory)
#include "cognitive/nimcp_self_model.h"               // Phase 12: Explicit Self-Model (identity, beliefs, capabilities)
#include "cognitive/nimcp_personality.h"              // Phase 12: Personality, Gender, and Sexual Identity
#include "core/brain/cognitive/nimcp_brain_cognitive.h" // Extracted cognitive systems module
#include "core/brain/biological/nimcp_brain_biological.h" // Biological subsystems (glial, neuromodulators, multimodal)
#include "core/brain/accessors/nimcp_brain_accessors.h" // Extracted accessor functions module
#include "core/brain/analysis/nimcp_brain_topology.h"  // Extracted topology/graph analysis module

// Cognitive Transcript (captures all stage outputs for response composition)
#include "core/brain/nimcp_cognitive_transcript.h"

// Edge-Cloud Hybrid Inference
#include "middleware/cloud/nimcp_cloud_inference.h"

// === COGNITIVE MODULE ENGINES (Decision Pipeline Integration) ===
#include "cognitive/recursive/nimcp_rcog_engine.h"       // Recursive Cognition Engine
#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h" // Inner Dialogue Engine
#include "cognitive/reasoning/nimcp_reasoning_chain.h"    // Reasoning Engine
#include "cognitive/collective_cognition/nimcp_collective_cognition.h" // Collective Cognition

// Imagination Engine: Forward declarations to avoid type conflicts with
// audio_cortex_t, visual_training_state_t, etc. already defined above.
// The full header is included in nimcp_brain_init_cognitive_engines.c.
#ifndef NIMCP_IMAGINATION_ENGINE_H
typedef struct imagination_engine imagination_engine_t;
typedef struct imagination_scenario imagination_scenario_t;
#define IMAGINATION_MODE_PROSPECTIVE 3  // From imagination_mode_t enum
extern imagination_scenario_t* imagination_begin_scenario(
    imagination_engine_t* engine, int mode, const void* goal);
extern int imagination_step_scenario(
    imagination_engine_t* engine, imagination_scenario_t* scenario);
extern int imagination_end_scenario(
    imagination_engine_t* engine, imagination_scenario_t* scenario);
extern void imagination_engine_destroy(imagination_engine_t* engine);
#endif

// === PHASE E: HIGHER-ORDER COGNITIVE & SOCIAL SYSTEMS ===
#include "cognitive/nimcp_shadow_emotions.h"   // Phase E5: Shadow Emotions (jealousy, envy, obsession, hubris, greed, narcissism)
#include "cognitive/nimcp_bias_detection.h"    // Phase E6: Bias Detection & Correction (racial, LGBTQ+, gender, misogyny, etc.)
// Network Analysis Module - uses topology module's community_structure_t to avoid conflicts
#include "cognitive/analysis/nimcp_network_analysis.h"

// === PHASE E: FULL EMOTIONAL INTELLIGENCE ===
#include "cognitive/nimcp_grief_and_loss.h"            // Phase E1: Grief, Loss, Bereavement (negative emotion)
#include "cognitive/nimcp_joy_euphoria.h"              // Phase E2: Joy, Euphoria, Value-Aligned Success (positive emotion)
#include "cognitive/nimcp_remorse_regret.h"            // Phase E3: Remorse, Regret, Evaluative Emotions (moral emotion)

// === PHASE C4: INFORMATION THEORY (SHANNON'S LAW) ===
#include "information/nimcp_shannon.h"                 // Phase C4: Shannon information theory for capacity/bottleneck analysis
#include "utils/quantum/nimcp_quantum_shannon.h"       // Phase C4.1: Quantum-Shannon diffusion for √N speedup
#include "information/nimcp_cross_modal.h"             // Phase C4.7: Cross-modal information flow tracking
#include "core/brain/information/nimcp_brain_shannon.h" // Shannon API module (extracted functions)
#include "cognitive/nimcp_love_loyalty_friendship.h"   // Phase E4: Love, Loyalty, Friendship (positive social emotion)
#include "cognitive/nimcp_empathetic_response.h"       // W9-finish: activate empathetic_response pipeline
#include "cognitive/nimcp_emotional_learning.h"        // W9-finish: read arousal for empathy dispatch
/* W9-finish: middleware_controller_on_pattern_match via extern —
 * cannot include the full header here because it forward-declares
 * pattern_library_t which collides with the unrelated same-named type in
 * cognitive/immune/nimcp_heal_patterns.h (transitively included via
 * self_heal.h). */
struct middleware_controller;
extern void middleware_controller_on_pattern_match(
    struct middleware_controller* controller, uint32_t pattern_id,
    float similarity, uint32_t region_id);

// Phase 11 Enhancement C1.1: Quantum Annealing for Weight Optimization
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

// Phase IS-1: Blood-Brain Barrier (BBB) cleanup
#include "security/nimcp_blood_brain_barrier.h"
#include "core/brain/factory/init/nimcp_brain_init.h"  // For nimcp_bbb_release_global_system

//=============================================================================
// Thread-local PRNG (thread-safe replacement for rand())
//=============================================================================
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_buffer_constants.h"

// Multi-network inference includes (CNN / SNN / LNN forward passes)
#include "training/nimcp_cnn_training.h"
#include "snn/nimcp_snn_network.h"
/* Forward-declare to avoid header conflict (thalamic_router_t typedef clash) */
struct snn_routing_bridge_s;
int snn_routing_bridge_update(struct snn_routing_bridge_s* bridge, float dt);
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "audio/synthesis/nimcp_formant_synth.h"
#include "utils/tensor/nimcp_tensor.h"
#include "core/brain/nimcp_brain_parallel_stages.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "training/nimcp_unified_training.h"  /* EMA swap for inference */
#include "memory/nimcp_memory_store.h"        /* Persistent memory recall fallback */
#include "memory/nimcp_memory_oodb.h"         /* OODB in-memory cache */
#include "cognitive/nimcp_ood_detector.h"     /* Out-of-distribution detection */

//=============================================================================
// Bio-Async Module Registration
//=============================================================================

static bio_module_context_t g_brain_bio_ctx = NULL;
static bool g_brain_bio_initialized = false;
static nimcp_platform_once_t g_brain_bio_once = NIMCP_PLATFORM_ONCE_INIT;
static nimcp_error_t g_brain_bio_init_result = NIMCP_SUCCESS;

// P1-9 FIX: Reference counter for bio-async context.
// Only unregister when last brain is destroyed.
// NOTE: NOT static - nimcp_brain_factory.c references this via extern declaration.
volatile int g_brain_bio_ref_count = 0;

// P3-50: Named constant for loss history circular buffer size
#define LOSS_HISTORY_SIZE 10

// P3-51: Named constant for confidence normalization factor
#define CONFIDENCE_NORMALIZATION_FACTOR 10.0F

//=============================================================================
// Forward Declarations - Strategy Pattern
//=============================================================================

typedef struct task_strategy task_strategy_t;

//=============================================================================
// Internal Brain Structure with Strategy
//=============================================================================

/**
 * @brief Brain internal structure with strategy pattern
 *
 * WHY: Encapsulates network, config, labels, stats, and task strategy
 * Enables type-specific behaviors without switch statements
 *
 * COMPLEXITY: O(1) access to all members
 */

//=============================================================================
// Forward Declarations
//=============================================================================

// Phase 2: COW helper - must be declared before brain_get_network()
bool ensure_writable_network(brain_t brain);

//=============================================================================
// Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
//=============================================================================
extern void nimcp_health_agent_heartbeat_ex(struct nimcp_health_agent* agent,
                                             const char* operation,
                                             float progress);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * ARCHITECTURE NOTE:
 * Error handling has been centralized to avoid duplicate thread-local storage.
 *
 * - set_error()              -> src/core/brain/strategy/nimcp_brain_strategy.c
 * - brain_get_last_error()   -> src/core/brain/strategy/nimcp_brain_strategy.c
 * - brain_clear_error()      -> src/core/brain/strategy/nimcp_brain_strategy.c
 *
 * All brain modules use the same thread-local error storage via external linkage.
 */
extern void set_error(const char* format, ...);

//=============================================================================
// Internal Access API (NIMCP 2.5 Consciousness Subsystems)
//=============================================================================

/**
 * WHAT: Get underlying adaptive network from brain
 * WHY: Introspection/salience/consolidation need direct network access
 * HOW: Return internal network handle
 *
 * WARNING: Exposes internals - for consciousness subsystems only!
 */

/**
 * WHAT: Get brain regions module from brain
 * WHY:  Middleware and other subsystems need access to brain region hierarchy
 * HOW:  Returns pointer to brain_module_t, no COW needed (per-brain state)
 *
 * SAFETY: Brain regions module is not shared (unlike network), so no COW needed
 */
brain_module_t* brain_get_brain_regions(brain_t brain)
{
    if (!brain) {
        set_error("NULL brain passed to brain_get_brain_regions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_brain_regions: brain is NULL");
        return NULL;
    }

    return brain->brain_regions;
}

//=============================================================================
// Size Presets - Builder Helpers
//=============================================================================

/**
 * @brief Get neuron count for size preset
 *
 * WHY: Abstracts size->neuron mapping for maintainability
 * Centralizes sizing logic
 *
 * COMPLEXITY: O(1)
 *
 * @param size Brain size preset
 * @return Neuron count for size
 */
static uint32_t get_neuron_count(brain_size_t size)
{
    switch (size) {
        case BRAIN_SIZE_TINY:
            return 100;
        case BRAIN_SIZE_SMALL:
            return 500;
        case BRAIN_SIZE_MEDIUM:
            return 1000;  // Reduced from 10000 for faster tests (1.8GB→180MB)
        case BRAIN_SIZE_LARGE:
            return 5000;  // Reduced from 100000 (9GB→450MB)
        case BRAIN_SIZE_CUSTOM:
            return 1000;
        default:
            return 1000;
    }
}

/**
 * @brief Get default sparsity target for size
 *
 * WHY: Larger networks need higher sparsity for efficiency
 * Balances performance and memory
 *
 * COMPLEXITY: O(1)
 *
 * @param size Brain size preset
 * @return Sparsity target (0.0-1.0)
 */

//=============================================================================
// Decision Caching
//=============================================================================

// Forward declarations
brain_decision_t* copy_decision(brain_decision_t* source);
brain_decision_t* copy_decision_deep(const brain_decision_t* source);

/**
 * @brief Check if input matches cached input
 *
 * WHY: Avoid redundant computation for repeated inputs
 * Common in batch processing and validation
 *
 * COMPLEXITY: O(n) where n = num_features
 * OPTIMIZATION: Early exit on first mismatch
 *
 * @param brain Brain handle
 * @param features Input to check
 * @param num_features Feature count
 * @return true if cached input matches
 */

/**
 * @brief Maximum wait time for mutex operations (in microseconds)
 *
 * WHY: Prevent indefinite blocking on mutex acquisition
 * HOW: Use timeout-based try-lock with retry loop
 */
#define MUTEX_TIMEOUT_US 5000000  // 5 seconds

/**
 * @brief Attempt to lock mutex with timeout and exponential backoff
 *
 * WHAT: Acquires mutex with timeout protection to prevent deadlocks
 * WHY:  Standard mutex_lock can block indefinitely causing deadlock
 * HOW:  Use trylock with exponential backoff up to max timeout
 *
 * RECOVERY: If lock cannot be acquired within timeout, logs critical error
 * and returns failure. Caller must handle gracefully (not proceed with
 * locked resource access).
 *
 * @param mutex Mutex to lock
 * @param timeout_us Maximum time to wait in microseconds
 * @return 0 on success, -1 on timeout
 */


// Forward declarations for static functions (SRP split)
static void brain_bio_init_once_routine(void);
static nimcp_error_t brain_bio_init(void);
static void brain_publish_state_event(bio_message_type_t event_type, uint32_t neuron_count, nimcp_bio_channel_type_t channel);
static void brain_publish_processing_event(const char* processing_type, float confidence);
static float strategy_classification_lr(void);
static void strategy_classification_transform(float* output, uint32_t size);
static float strategy_classification_loss(const float* pred, const float* target, uint32_t size);
static float strategy_regression_lr(void);
static void strategy_regression_transform(float* output, uint32_t size);
static float strategy_regression_loss(const float* pred, const float* target, uint32_t size);
static float strategy_pattern_lr(void);
static void strategy_pattern_transform(float* output, uint32_t size);
static float strategy_pattern_loss(const float* pred, const float* target, uint32_t size);
static float strategy_association_lr(void);
static void strategy_association_transform(float* output, uint32_t size);
static float strategy_association_loss(const float* pred, const float* target, uint32_t size);
static task_strategy_t* strategy_create(brain_task_t task);
static void strategy_destroy(task_strategy_t* strategy);
static float get_default_sparsity(brain_size_t size);
static adaptive_spike_params_t build_spike_params(float sparsity_target);
static network_config_t build_base_network_config(uint32_t num_inputs, uint32_t num_outputs, uint32_t num_neurons, ode_integration_method_t integration_method);
static adaptive_network_config_t build_network_config(uint32_t num_inputs, uint32_t num_outputs, uint32_t num_neurons, float sparsity_target, ode_integration_method_t integration_method);
static void init_brain_config(brain_config_t* config, const char* task_name, brain_size_t size, brain_task_t task, uint32_t num_inputs, uint32_t num_outputs, task_strategy_t* strategy);
static bool is_cached_input(brain_t brain, const float* features, uint32_t num_features);
static void cache_decision(brain_t brain, const float* features, uint32_t num_features, brain_decision_t* decision);
static int mutex_lock_with_timeout(nimcp_platform_mutex_t* mutex, uint64_t timeout_us);
static void force_unlock_with_logging(nimcp_platform_mutex_t* mutex, const char* context);
static bool validate_creation_params(const char* task_name, uint32_t num_inputs, uint32_t num_outputs);
static personality_profile_t* create_personality(const brain_config_t* config);
static uint32_t get_or_create_label_index(brain_t brain, const char* label);
static void label_to_output(brain_t brain, const char* label, float* output, float confidence);
static void adapt_learning_rate_from_loss(brain_t brain, float current_loss);
static float quantum_weight_energy(const float* weights, uint32_t dim, void* user_data);
brain_decision_t* allocate_decision(uint32_t output_size);
uint32_t perform_forward_pass(brain_t brain, const float* features, uint32_t num_features, brain_decision_t* decision);
static void determine_output_label(brain_t brain, brain_decision_t* decision);
static void populate_interpretability(brain_t brain, const float* features, uint32_t num_features, uint32_t active_neurons, brain_decision_t* decision);
static void update_inference_stats(brain_t brain, brain_decision_t* decision);
static action_t brain_decision_to_action(const brain_decision_t* decision, uint32_t action_id, const char* action_name);
static action_t features_to_action(const float* features, uint32_t num_features, uint32_t agent_id);
static bool save_working_memory_state(working_memory_t* wm, FILE* file);
static bool save_metadata(brain_t brain, const char* filepath);
static bool load_working_memory_item(working_memory_t* wm, FILE* file);
static bool load_working_memory_state(brain_t brain, FILE* file);
static bool load_metadata(brain_t brain, const char* filepath);
static bool ensure_snapshot_dir(const char* snapshot_dir);
static const char* get_snapshot_dir(brain_t brain);

// Forward declarations for functions defined in later _part_*.c files
// (needed because _part_lifecycle.c calls clear_cache from _part_core.c,
//  and _part_helpers.c calls init_brain_stats from _part_stats.c)
void clear_cache(brain_t brain);
void init_brain_stats(brain_stats_t* stats, const char* task_name, brain_size_t size,
                      uint32_t num_inputs, float learning_rate);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_brain_part_lifecycle.c"  // 6 functions: lifecycle
#include "nimcp_brain_part_helpers.c"  // 37 functions: helpers
#include "nimcp_brain_part_processing.c"  // 2 functions: processing
#include "nimcp_brain_part_accessors.c"  // 12 functions: accessors
#include "nimcp_brain_part_stats.c"  // 2 functions: stats
#include "core/brain/nimcp_cognitive_dispatch.h"  // Parallel dispatch types
#include "nimcp_brain_part_core.c"  // 14 functions: core
#include "nimcp_brain_part_io.c"  // 4 functions: io
