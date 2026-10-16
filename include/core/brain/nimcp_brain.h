//=============================================================================
// nimcp_brain.h - High-Level Brain API (Application-Friendly)
//=============================================================================

#ifndef NIMCP_BRAIN_H
#define NIMCP_BRAIN_H

#include "plasticity/adaptive/nimcp_adaptive.h"
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "common/nimcp_export.h"
#include "core/neuron_models/nimcp_neuron_model.h"  // For ode_integration_method_t
#include "information/nimcp_shannon.h"  // Phase C4: Shannon Information Theory
#include "utils/quantum/nimcp_quantum_shannon.h"  // Phase C4.1: Quantum-Shannon diffusion
#include "information/nimcp_cross_modal.h"  // Phase C4.7: Cross-modal information flow
#include "async/nimcp_future.h"  // Async futures for non-blocking operations
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for opaque types (full headers only in .c file)
// Using void* to avoid typedef conflicts between modules
// The .c file will include headers and cast appropriately

// === PHASE 10: ADVANCED COGNITIVE SYSTEMS ===

// Phase 10.1: Working Memory
typedef struct working_memory working_memory_t;

// Phase M2: Systems Consolidation
typedef struct systems_consolidation_system systems_consolidation_system_t;

// Phase 10.2: Emotional Tagging
typedef struct emotional_system emotional_system_t;
typedef struct emotional_tag emotional_tag_t;
typedef struct emotional_state emotional_state_t;

// Phase 10.3: Executive Functions
typedef struct executive_controller executive_controller_t;

// Phase 10.4: Sleep-Wake Cycle
typedef struct sleep_system_struct* sleep_system_t;

// Phase 10.5: Mental Health Monitoring
typedef struct mental_health_monitor mental_health_monitor_t;

// Phase 10.6: Theory of Mind
// Forward declare Theory of Mind (opaque pointer)
#ifndef NIMCP_THEORY_OF_MIND_T_DEFINED
typedef struct theory_of_mind_s* theory_of_mind_t;
#define NIMCP_THEORY_OF_MIND_T_DEFINED
#endif

// Phase 10.7: Natural Explanations
typedef struct explanation_generator_s* explanation_generator_t;  // Opaque pointer
typedef struct natural_explanation natural_explanation_t;

// Phase 10.8: Meta-Learning
typedef struct meta_learner_s* meta_learner_t;  // Opaque pointer

// Phase 10.9: Predictive Processing
typedef struct predictive_network_s* predictive_network_t;  // Opaque pointer

// Phase 10.11: Mirror Neurons (Social Cognition & Imitation Learning)
typedef struct mirror_neurons_system* mirror_neurons_t;  // Opaque pointer

// Phase 7.2: Parietal Lobe (Mathematical/Scientific Reasoning)
#ifndef NIMCP_PARIETAL_LOBE_T_DEFINED
#define NIMCP_PARIETAL_LOBE_T_DEFINED
typedef struct parietal_lobe parietal_lobe_t;  // Opaque pointer
#endif

// Phase 7.3: Dragonfly (Bio-inspired Target Tracking and Interception)
typedef struct dragonfly_system_s dragonfly_system_t;  // Opaque pointer
typedef struct dragonfly_medulla_bridge_s* dragonfly_medulla_bridge_t;  // Dragonfly-Medulla bridge
typedef struct dragonfly_medulla_modulation_s dragonfly_medulla_modulation_t;  // Dragonfly-Medulla modulation state

// Medulla Oblongata (Brainstem regulation)
typedef struct medulla_struct* medulla_t;  // Opaque pointer

// Knowledge Graph Reader (Self-Awareness - Runtime introspection of NIMCP structure)
typedef struct kg_reader kg_reader_t;  // Opaque pointer

// Global Workspace Architecture (Global Workspace Theory - Baars, Dehaene)
#ifndef GLOBAL_WORKSPACE_T_DEFINED
#define GLOBAL_WORKSPACE_T_DEFINED
typedef struct global_workspace_struct* global_workspace_t;  // Opaque pointer
#endif

// Neuromodulator System (for mental health interventions)
typedef struct neuromodulator_system_struct* neuromodulator_system_t;  // Opaque pointer

// Brain Regions Module (for brain region hierarchy with cortical layers)
// Forward-declared as opaque struct pointer for accessor function
struct brain_module_struct;

// Phase 9.4: Symbolic Logic & Reasoning
typedef struct symbolic_logic symbolic_logic_t;

// Pink Noise Neuromodulator - typedef defined in plasticity/neuromodulators/nimcp_neuromod_pink_noise.h

// === PHASE 7: ADVANCED SUBSYSTEM ACCESSORS ===
// Forward declarations for opaque pointers used by accessor functions
// Note: glial_integration_t already defined via nimcp_distributed_cognition.h
typedef struct brain_oscillation_analyzer_struct brain_oscillation_analyzer_t;
typedef struct introspection_context_struct* introspection_context_t;
typedef struct ethics_engine_struct* ethics_engine_t;
typedef struct salience_evaluator_struct* salience_evaluator_t;
typedef struct consolidation_handle_struct* consolidation_handle_t;
typedef struct curiosity_engine_struct* curiosity_engine_t;
typedef struct knowledge_system_struct* knowledge_system_t;

// === PHASE E: HIGHER-ORDER COGNITIVE & SOCIAL SYSTEMS ===
// Forward declarations - actual typedefs in respective headers
// Phase E5: Shadow Emotions - defined in cognitive/nimcp_shadow_emotions.h
// Phase E6: Bias Detection - defined in cognitive/nimcp_bias_detection.h

/**
 * @file nimcp_brain.h
 * @brief Simple, high-level API for creating lightweight learning systems
 *
 * This is the recommended API for application developers. It provides:
 * - Simple creation with sensible defaults
 * - Easy pattern learning from examples or LLMs
 * - Fast inference (<1ms for small models)
 * - Serialization for persistence
 * - Interpretability for debugging
 *
 * Example usage:
 * ```c
 * // Create a small brain for ethics decisions
 * brain_t brain = brain_create("ethics", BRAIN_SIZE_SMALL);
 *
 * // Learn from examples
 * brain_learn_example(brain, situation_features, "allow", 0.95);
 *
 * // Make decisions
 * brain_decision_t decision = brain_decide(brain, new_situation);
 * printf("Decision: %s (confidence: %.2f)\n",
 *        decision.label, decision.confidence);
 *
 * // Save for later
 * brain_save(brain, "ethics_brain.nimcp");
 * ```
 *
 * =============================================================================
 * THREAD SAFETY AND CONCURRENCY MODEL
 * =============================================================================
 *
 * The NIMCP brain API provides varying levels of thread safety depending on
 * the operation type. This section documents the concurrency guarantees and
 * requirements for each category of operations.
 *
 * ## Thread-Safe Operations (No External Synchronization Required)
 *
 * The following operations are fully thread-safe and can be called concurrently
 * from multiple threads on the SAME brain instance:
 *
 * - brain_decide() - Multiple threads can perform inference concurrently.
 *   Uses internal mutex-protected caching with timeout protection.
 * - brain_get_stats() - Read-only statistics access.
 * - brain_get_config() - Read-only configuration access.
 * - brain_clone_cow() - Creates COW clone using atomic reference counting.
 * - brain_get_*() accessor functions - Read-only subsystem access.
 *
 * ## Serialized Operations (Internal Synchronization, One-at-a-Time)
 *
 * These operations use internal mutexes and will serialize if called
 * concurrently. Safe to call from multiple threads, but performance may
 * suffer under high contention:
 *
 * - brain_learn_example() - Learning modifies network weights. Internal
 *   cache mutex ensures consistency, but concurrent learning may see
 *   interleaved weight updates. For best results, batch learning calls
 *   or use external synchronization.
 * - brain_learn_batch() - Same as brain_learn_example(), processes batch
 *   sequentially.
 *
 * ## Requires External Synchronization
 *
 * These operations are NOT thread-safe and require caller-provided
 * synchronization (e.g., external mutex) if called concurrently:
 *
 * - brain_create*() - Creation should be single-threaded or externally
 *   synchronized if creating multiple brains sharing resources.
 * - brain_destroy() - Must not be called while other threads are using
 *   the brain. Ensure all operations complete before destruction.
 * - brain_resize() - Network modification. Exclusive access required.
 * - brain_save() / brain_load() - File I/O is not thread-safe.
 * - brain_observe_action() - Mirror neuron updates require serialization.
 *
 * ## Concurrent brain_learn and brain_decide Behavior
 *
 * When brain_learn_example() and brain_decide() are called concurrently:
 *
 * 1. Learning may invalidate cached decisions (cache is cleared after
 *    weight updates).
 * 2. Inference during learning may see partially updated weights.
 * 3. Results are still valid (no crashes/corruption), but may be
 *    inconsistent - a decision made during learning may differ from
 *    one made immediately after.
 *
 * For applications requiring consistent read-after-write semantics,
 * use external synchronization:
 *
 * ```c
 * nimcp_mutex_lock(&app_mutex);
 * brain_learn_example(brain, features, label, confidence);
 * brain_decision_t* decision = brain_decide(brain, features, num);
 * nimcp_mutex_unlock(&app_mutex);
 * ```
 *
 * ## Reference Counting for COW Clones
 *
 * Copy-on-Write (COW) clones use lock-free atomic reference counting:
 * - Multiple clones can be created/destroyed concurrently.
 * - Shared network is destroyed when last reference is released.
 * - No mutex required for refcount operations (C11 atomics).
 * - Writing to a COW clone triggers deep copy (not thread-safe).
 *
 * ## Deadlock Protection
 *
 * All internal mutex acquisitions use timeout-based locking (5 second
 * default) to prevent deadlocks. If a mutex cannot be acquired within
 * the timeout, the operation fails gracefully with an error rather than
 * blocking indefinitely.
 *
 * ## Global Statistics
 *
 * Global statistics counters use atomic operations for thread safety.
 * No external synchronization is required for statistics access.
 *
 * =============================================================================
 */

//=============================================================================
// Brain Sizes & Presets
//=============================================================================

/**
 * @brief Pre-configured brain sizes
 */
typedef enum {
    BRAIN_SIZE_MICRO,  /**< 25 neurons, <100KB, ~0.05ms inference (unit tests, swarm drones) */
    BRAIN_SIZE_TINY,   /**< 100 neurons, <1MB,  ~0.1ms inference */
    BRAIN_SIZE_SMALL,  /**< 500 neurons, ~5MB,  ~0.3ms inference */
    BRAIN_SIZE_MEDIUM, /**< 1K neurons,  ~10MB, ~0.5ms inference */
    BRAIN_SIZE_LARGE,  /**< 5K neurons,  ~50MB, ~2ms inference */
    BRAIN_SIZE_CUSTOM  /**< User-defined size */
} brain_size_t;

/**
 * @brief Brain task templates
 */
typedef enum {
    BRAIN_TASK_CLASSIFICATION,   /**< Multi-class classification */
    BRAIN_TASK_REGRESSION,       /**< Continuous value prediction */
    BRAIN_TASK_PATTERN_MATCHING, /**< Pattern recognition */
    BRAIN_TASK_SEQUENCE,         /**< Temporal sequence learning */
    BRAIN_TASK_ASSOCIATION,      /**< Association learning (Hebbian) */
    BRAIN_TASK_CUSTOM            /**< Custom task */
} brain_task_t;

/**
 * @brief Brain initialization mode — controls subsystem init depth
 *
 * WHAT: Controls how many subsystems are initialized at brain creation
 * WHY:  Large brains (>100K neurons) can take minutes to create with full init
 * HOW:  FAST mode initializes only core + training, defers everything else
 *
 * | Mode    | Subsystems initialized       | Creation time (1.5M neurons) |
 * |---------|------------------------------|------------------------------|
 * | FULL    | All 80+ subsystems           | ~150-250s                    |
 * | FAST    | Core + GPU + training only   | ~30-60s                      |
 * | MINIMAL | Neural network only          | ~10-20s                      |
 *
 * FAST mode initializes: neural network, GPU context, GPU inference, security,
 * immune system, training pipeline, plasticity coordinator, signal handler.
 * All other subsystems are deferred to first use (lazy initialization).
 *
 * All modes support the full API — deferred subsystems are initialized on
 * first access. FAST mode is recommended for large brains in production.
 */
typedef enum {
    BRAIN_INIT_FULL    = 0,  /**< Initialize all subsystems (default) */
    BRAIN_INIT_FAST    = 1,  /**< Core + GPU + training only, everything else lazy */
    BRAIN_INIT_MINIMAL = 2   /**< Neural network only, no subsystems */
} brain_init_mode_t;

//=============================================================================
// Configuration Profiles
//=============================================================================

/**
 * @brief Pre-defined brain configuration profiles
 *
 * WHAT: Configuration profiles replace 90+ boolean flags with semantic presets
 * WHY:  Simplify brain configuration for common use cases
 * HOW:  Use brain_config_from_profile() to get a pre-configured brain_config_t
 *
 * USAGE:
 * ```c
 * brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
 * config.size = BRAIN_SIZE_MEDIUM;
 * config.num_inputs = 128;
 * config.num_outputs = 10;
 * strncpy(config.task_name, "my_task", sizeof(config.task_name) - 1);
 * brain_t brain = brain_create_custom(&config);
 * ```
 */
typedef enum {
    /**
     * BRAIN_CONFIG_MINIMAL - Bare minimum for inference
     * USE CASES: Unit tests, swarm drones, embedded systems, benchmarks
     * ENABLED: Core neural network, basic learning (STDP/Hebbian), memory pools
     * DISABLED: All cognitive subsystems, sensory cortices, security
     * PERFORMANCE: Init <100ms, Memory <10MB, Inference <1ms
     */
    BRAIN_CONFIG_MINIMAL = 0,

    /**
     * BRAIN_CONFIG_STANDARD - Common features for general use
     * USE CASES: Production applications, basic cognitive features, decision making
     * ENABLED: MINIMAL + working memory, global workspace, introspection, salience
     * DISABLED: Sensory cortices, ethics, theory of mind, advanced security
     * PERFORMANCE: Init <500ms, Memory 20-50MB, Inference 1-5ms
     */
    BRAIN_CONFIG_STANDARD,

    /**
     * BRAIN_CONFIG_COGNITIVE - Full cognitive systems
     * USE CASES: Consciousness simulation, social cognition, ethical reasoning
     * ENABLED: STANDARD + theory of mind, mirror neurons, ethics, emotions
     * DISABLED: Sensory cortices (enable as needed), multi-GPU, quantum features
     * PERFORMANCE: Init 1-3s, Memory 50-200MB, Inference 5-20ms
     */
    BRAIN_CONFIG_COGNITIVE,

    /**
     * BRAIN_CONFIG_RESEARCH - All features for research
     * USE CASES: Scientific research, full biological realism, feature exploration
     * ENABLED: ALL available features including sensory, security, quantum
     * PERFORMANCE: Init 5-30s, Memory 500MB-5GB, Inference 20-100ms
     */
    BRAIN_CONFIG_RESEARCH,

    /**
     * BRAIN_CONFIG_EMBEDDED - Optimized for constrained devices
     * USE CASES: IoT devices, edge computing, battery-powered, latency-critical
     * ENABLED: Core network, basic learning, reduced working memory, lazy init
     * PERFORMANCE: Init <200ms, Memory <20MB, Inference <2ms
     */
    BRAIN_CONFIG_EMBEDDED,

    BRAIN_CONFIG_PROFILE_COUNT  /**< Number of configuration profiles */
} brain_config_profile_t;

//=============================================================================
// Glial Cell Ratio Constants (Biological Documentation)
//=============================================================================

/**
 * Glial cell to neuron ratios based on biological research
 *
 * REFERENCES:
 * - Azevedo et al. (2009): Human brain has ~1:1 glia-to-neuron ratio
 * - Herculano-Houzel (2014): Glia/neuron ratio varies by brain region
 * - von Bartheld et al. (2016): ~86B neurons, ~85B glia in human brain
 *
 * ASTROCYTES (ratio = 5): ~20% of glial cells, tripartite synapses
 * OLIGODENDROCYTES (ratio = 7): ~14% of glial cells, myelin sheaths
 * MICROGLIA (ratio = 10): ~10% of glial cells, immune response
 */
#define GLIA_ASTROCYTE_RATIO            5   /**< neurons/5 = astrocyte count */
#define GLIA_OLIGODENDROCYTE_RATIO      7   /**< neurons/7 = oligodendrocyte count */
#define GLIA_MICROGLIA_RATIO            10  /**< neurons/10 = microglia count */

/** Calculate glial cell counts from neuron count */
#define GLIA_ASTROCYTE_COUNT(n)       ((n) / GLIA_ASTROCYTE_RATIO)
#define GLIA_OLIGODENDROCYTE_COUNT(n) ((n) / GLIA_OLIGODENDROCYTE_RATIO)
#define GLIA_MICROGLIA_COUNT(n)       ((n) / GLIA_MICROGLIA_RATIO)

//=============================================================================
// Configuration Validation Types
//=============================================================================

/**
 * @brief Configuration validation error codes
 */
typedef enum {
    BRAIN_CONFIG_OK = 0,              /**< Configuration is valid */
    BRAIN_CONFIG_ERR_NULL,            /**< NULL config pointer */
    BRAIN_CONFIG_ERR_INVALID_SIZE,    /**< Invalid brain size */
    BRAIN_CONFIG_ERR_INVALID_TASK,    /**< Invalid task type */
    BRAIN_CONFIG_ERR_ZERO_INPUTS,     /**< num_inputs is zero */
    BRAIN_CONFIG_ERR_ZERO_OUTPUTS,    /**< num_outputs is zero */
    BRAIN_CONFIG_ERR_LEARNING_RATE,   /**< learning_rate out of range */
    BRAIN_CONFIG_ERR_SPARSITY,        /**< sparsity_target out of range */
    BRAIN_CONFIG_ERR_DEPENDENCY,      /**< Feature dependency not met */
    BRAIN_CONFIG_ERR_INCOMPATIBLE,    /**< Incompatible feature combination */
    BRAIN_CONFIG_ERR_RESOURCE,        /**< Resource constraints violated */
} brain_config_error_code_t;

/**
 * @brief Detailed configuration validation error
 */
typedef struct {
    brain_config_error_code_t code;   /**< Error code */
    char message[256];                /**< Human-readable error message */
    const char* field_name;           /**< Name of the invalid field */
    const char* dependency;           /**< Missing dependency (if ERR_DEPENDENCY) */
} brain_config_error_t;

//=============================================================================
// Brain Handle & Configuration
//=============================================================================

/**
 * @brief Opaque brain handle
 */
#ifndef NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#define NIMCP_BRAIN_T_DEFINED
#endif

/**
 * @brief Forward declaration for brain stats (full definition below)
 */
typedef struct brain_stats_struct brain_stats_t;

/**
 * @brief Astrocyte network statistics
 */
typedef struct {
    uint32_t num_astrocytes;           /**< Total astrocytes in network */
    float avg_calcium_um;              /**< Average calcium level (µM) */
    uint32_t num_tripartite_synapses;  /**< Synapses with astrocyte modulation */
    uint64_t total_modulations;        /**< Total synaptic modulations applied */
    float avg_modulation_strength;     /**< Average modulation factor (0.8-1.2) */
} astrocyte_stats_t;

//=============================================================================
// Persistence Format Versioning
//=============================================================================

/**
 * @brief Brain metadata file format version header
 *
 * WHAT: Version identifier written at start of .meta files
 * WHY:  Enable backward compatibility and format evolution
 * HOW:  Check magic + version on load, support migrations
 *
 * FORMAT: [magic:4 bytes][major:2][minor:2][flags:4][reserved:4]
 * Total: 16 bytes header
 *
 * NOTE: SHA-256 checksum field will be added in v1.1 when CHECKSUM flag is set
 */
typedef struct {
    uint8_t magic[4];       /**< Magic bytes: {'N', 'I', 'M', 'P'} */
    uint16_t version_major; /**< Major version (incompatible changes) */
    uint16_t version_minor; /**< Minor version (compatible changes) */
    uint32_t flags;         /**< Format flags (compressed, encrypted, etc.) */
    uint32_t reserved;      /**< Reserved for future use */
} nimcp_file_header_t;

// Current format version
#define NIMCP_FORMAT_VERSION_MAJOR 1
#define NIMCP_FORMAT_VERSION_MINOR 0

// Header magic bytes
#define NIMCP_MAGIC_0 'N'
#define NIMCP_MAGIC_1 'I'
#define NIMCP_MAGIC_2 'M'
#define NIMCP_MAGIC_3 'P'

// Format flags
#define NIMCP_FORMAT_FLAG_COMPRESSED  0x00000001  /**< Data is compressed */
#define NIMCP_FORMAT_FLAG_ENCRYPTED   0x00000002  /**< Data is encrypted */
#define NIMCP_FORMAT_FLAG_CHECKSUM    0x00000004  /**< File has checksum */

/**
 * @brief Comprehensive brain configuration
 *
 * WHAT: Extended configuration for all advanced subsystems
 * WHY:  Enable full integration of consciousness, glial, sensory, and cognitive modules
 * HOW:  Optional flags allow selective feature activation
 */
typedef struct {
    // === CORE CONFIGURATION ===
    brain_size_t size;        /**< Brain size preset */
    brain_task_t task;        /**< Task template */
    uint32_t num_inputs;      /**< Input dimension */
    uint32_t num_outputs;     /**< Output dimension */
    float learning_rate;      /**< Learning rate (0.001-0.1) */
    float sparsity_target;    /**< Target sparsity (0.7-0.95) */
    bool enable_explanations; /**< Enable interpretability */
    char task_name[64];       /**< Name for this brain */

    // === NETWORK TYPE CONFIGURATION ===
    /**
     * Network architecture type for this brain
     *
     * WHAT: Specifies the primary neural network architecture
     * WHY:  Different architectures have different training and inference characteristics:
     *       - ADAPTIVE (default): General-purpose, backpropagation training
     *       - SNN: Spike-based processing, temporal coding, STDP/eProp training
     *       - LNN: Continuous-time dynamics, ODE-based, adjoint training
     *       - CNN: Spatial feature extraction, convolutional backprop
     *       - HYBRID: Multiple architectures with unified interface
     * HOW:  Set during brain creation, affects training dispatch
     *
     * NOTE: Changing this requires reinitializing the training context
     */
    uint8_t network_type;     /**< Network type (cast to nimcp_network_type_t) */

    // === MINIMAL MODE (Performance Optimization) ===
    /**
     * Minimal mode flag - disables all optional cognitive subsystems
     *
     * WHAT: When true, skips initialization of non-essential subsystems
     * WHY:  Provides 5-10x faster brain creation for tests and embedded systems
     * HOW:  Sets all enable_* flags to false during config initialization
     *
     * ENABLED SUBSYSTEMS in minimal mode:
     * - Core neural network (required)
     * - Basic learning (STDP, Hebbian)
     * - Memory pools (performance)
     *
     * DISABLED SUBSYSTEMS in minimal mode:
     * - Working memory, Theory of Mind, Mirror neurons
     * - Global Workspace, Emotional systems
     * - Glial integration, Myelin sheath
     * - Visual/Audio/Speech cortices
     * - Ethics, Empathy, Introspection
     * - Training integration, Predictive processing
     *
     * USE CASES:
     * - Unit/integration tests
     * - Swarm drone brains (resource-constrained)
     * - Benchmarking core inference speed
     *
     * EXAMPLE:
     * ```c
     * brain_config_t config = {0};
     * config.minimal_mode = true;  // Fast initialization
     * brain_t brain = brain_create_custom(&config);
     * ```
     */
    bool minimal_mode;        /**< Minimal mode - skip optional subsystems (default: false) */

    // === PART A: DIFFERENTIAL EQUATIONS & PDEs ===

    /**
     * ODE Integration Method (A1.x)
     *
     * WHAT: Numerical integration algorithm for neuron membrane dynamics
     * WHY: Control accuracy/speed tradeoff for differential equation solving
     * HOW: Passed to neuron update functions during network simulation
     *
     * OPTIONS:
     * - ODE_EULER (default): Fast, first-order accuracy
     *   - Use when: Real-time performance critical, dt < 0.5ms
     *   - Speed: 1.0× (baseline)
     *
     * - ODE_RK2: Balanced, second-order accuracy
     *   - Use when: Good accuracy needed, dt = 0.5-2ms
     *   - Speed: ~0.5× (2× slower than Euler)
     *
     * - ODE_RK4: Best accuracy, fourth-order
     *   - Use when: Scientific precision required, dt = 1-5ms
     *   - Speed: ~0.25× (4× slower than Euler)
     *
     * BACKWARD COMPATIBILITY:
     * - Default value is ODE_EULER (0)
     * - Existing code continues to use Euler method
     * - No breaking changes to API or behavior
     *
     * EXAMPLE:
     * ```c
     * brain_config_t config = brain_default_config(...);
     * config.neuron_integration = ODE_RK4;  // High accuracy
     * brain_t brain = brain_create_custom(&config);
     * ```
     */
    ode_integration_method_t neuron_integration;  /**< Neuron ODE integration method (default: ODE_EULER) */

    // === PHASE 3: DISTRIBUTED COGNITION ===
    bool enable_distributed;  /**< Enable P2P cognitive coordination */
    p2p_node_t p2p_node;      /**< P2P network node (if distributed) */

    // === PHASE 5/6: BIOLOGICAL REALISM ===
    bool enable_glial;        /**< Enable glial integration (astrocytes, oligodendrocytes, microglia) */
    bool enable_oscillations; /**< Enable brain wave analysis (delta, theta, alpha, beta, gamma) */
    bool enable_myelin_sheath; /**< Enable myelin sheath structural modeling */
    uint32_t num_astrocytes;  /**< Number of astrocytes (default: neurons/5) */
    uint32_t num_oligodendrocytes; /**< Number of oligodendrocytes (default: neurons/7) */
    uint32_t num_microglia;   /**< Number of microglia (default: neurons/10) */
    bool enable_microglia_pruning; /**< G5: allow microglia to mark synapses for pruning during learning (default: false — enable only after validation) */
    bool enable_glial_synaptic_modulation; /**< G3: apply astrocyte modulation to synaptic weights during forward pass (default: true when enable_glial) */
    bool enable_glial_myelin_conduction;   /**< G4: apply myelin conduction factor in propagation (default: true when enable_glial + enable_myelin_sheath) */

    // === LAZY INITIALIZATION (Performance optimization for all brain sizes) ===
    /**
     * WHAT: Defer expensive subsystem creation until first use
     * WHY:  Many use cases don't need all subsystems; lazy init saves 10-30s startup
     * HOW:  Subsystems are created on first access via brain_get_*() accessors
     *
     * LAZY-ENABLED SUBSYSTEMS (when lazy_init_mode=true):
     * - Visual/Audio/Speech cortices: Created on first sensory input
     * - Working memory: Created on first memory store/retrieve
     * - Theory of Mind: Created on first social reasoning call
     * - Global Workspace: Created on first conscious broadcast
     * - Ethics/Empathy: Created on first ethical evaluation
     * - Mirror neurons: Created on first observation-action mapping
     * - Executive functions: Created on first cognitive control request
     * - Consolidation: Created on first memory consolidation
     * - Meta-learning: Created on first learning rate adaptation
     *
     * ALWAYS EAGER (required for core functionality):
     * - Neural network (required for any inference)
     * - Event bus (required for subsystem coordination)
     * - Memory pools (hot-path performance)
     * - Strategy (task-specific behavior)
     */
    bool lazy_init_mode;      /**< Enable lazy initialization for ALL heavy subsystems (default: false) */
    bool lazy_dendrite_init;  /**< Defer dendrite network creation (default: false) */
    bool lazy_axon_init;      /**< Defer axon network creation (default: false) */
    bool lazy_visual_init;    /**< Defer visual cortex creation (default: false) */
    bool lazy_audio_init;     /**< Defer audio cortex creation (default: false) */
    bool lazy_speech_init;    /**< Defer speech cortex creation (default: false) */
    bool lazy_working_memory_init;  /**< Defer working memory creation (default: false) */
    bool lazy_theory_of_mind_init;  /**< Defer theory of mind creation (default: false) */
    bool lazy_global_workspace_init; /**< Defer global workspace creation (default: false) */
    bool lazy_ethics_init;    /**< Defer ethics engine creation (default: false) */
    bool lazy_mirror_neurons_init;  /**< Defer mirror neurons creation (default: false) */
    bool lazy_executive_init; /**< Defer executive controller creation (default: false) */
    bool lazy_consolidation_init;  /**< Defer consolidation system creation (default: false) */
    bool lazy_meta_learning_init;  /**< Defer meta-learning creation (default: false) */
    bool lazy_neuromod_init;  /**< Defer neuromodulator system creation (default: false) */
    bool lazy_glial_init;     /**< Defer glial integration creation (default: false) */
    bool lazy_cortical_init;  /**< Defer cortical columns creation until first use (default: false) */
    bool lazy_topographic_init; /**< Defer topographic maps creation until first use (default: false) */
    bool lazy_pr_memory_init; /**< Defer Prime Resonant memory creation (default: false) */
    bool enable_dendrites;    /**< Enable dendrite subsystem entirely (default: true) */
    bool enable_axons;        /**< Enable axon subsystem entirely (default: true) */
    bool enable_pr_memory;    /**< Enable Prime Resonant memory system (default: true) */

    // === PHASE 5.3: SENSORY PROCESSING ===
    bool enable_visual_cortex; /**< Enable visual cortex (V1) for image processing */
    bool enable_audio_cortex;  /**< Enable audio cortex (A1) for sound processing */
    bool enable_speech_cortex; /**< Enable speech cortex (STG/Wernicke) for language processing */

    // === CONSCIOUSNESS & COGNITION ===
    bool enable_introspection; /**< Enable self-awareness and uncertainty estimation */
    bool enable_ethics;        /**< Enable ethical reasoning (Golden Rule, empathy) */
    bool enable_salience;      /**< Enable fast attention/relevance evaluation */
    bool enable_consolidation; /**< Enable memory consolidation (sleep-like learning) */
    bool enable_curiosity;     /**< Enable exploration and knowledge gap detection */
    bool enable_knowledge;     /**< Enable multi-domain knowledge acquisition */
    bool enable_wellbeing;     /**< Enable distress detection and ethical safeguards */
    bool enable_logic;         /**< Enable symbolic logic and reasoning (Phase 9.4) */

    // === ADVANCED PLASTICITY ===
    bool enable_eligibility_traces; /**< Enable temporal credit assignment (Phase 5.1) */
    bool enable_pink_noise;    /**< Enable pink noise neuromodulation (Phase 4) */
    bool enable_spike_nlp;     /**< Enable NLP via spike encoding (Phase 5.1) */
    bool enable_fractal_topology; /**< Enable scale-free network topology (Phase 2) */

    // Attention Mechanism
    bool enable_multihead_attention;  /**< Enable attention mechanism for selective processing */
    uint32_t num_attention_heads;     /**< Number of attention heads (default: 8) */
    uint32_t attention_key_dim;       /**< Key/query dimension (default: 64) */
    bool enable_thalamic_gate;        /**< Enable thalamic gating for top-down control */
    bool enable_salience_weighting;   /**< Enable salience-based attention */

    // Brain Regions Architecture (hierarchical cortical organization)
    bool enable_brain_regions;        /**< Enable modular brain regions with cortical layers */
    uint32_t num_brain_regions;       /**< Number of brain regions to create (default: 4) */
    uint32_t neurons_per_region;      /**< Neurons per region (default: 1000) */

    // === PHASE CC-1: CORTICAL COLUMNS ARCHITECTURE (Tier 0.65) ===
    /**
     * Cortical Columns Architecture
     *
     * WHAT: Biologically-inspired columnar organization (Douglas & Martin 1991)
     * WHY:  Implements hierarchical processing with minicolumns (~80-100 neurons)
     *       and hypercolumns (~100K neurons) for feature detection
     * HOW:  Competition modes (winner-take-all, k-winners, softmax) with lateral
     *       inhibition, 6-layer laminar structure, and topographic maps
     */
    bool enable_cortical_columns;     /**< Enable cortical column architecture */
    uint32_t num_hypercolumns;        /**< Number of hypercolumns to create (default: 10) */
    uint32_t minicolumns_per_hypercolumn; /**< Minicolumns per hypercolumn (default: 100) */
    uint32_t neurons_per_minicolumn;  /**< Neurons per minicolumn (default: 80) */
    bool enable_laminar_structure;    /**< Enable 6-layer cortical organization */
    bool enable_columnar_connectivity; /**< Enable canonical microcircuit connectivity */
    bool enable_visual_topographic;   /**< Enable retinotopic map for V1 */
    bool enable_auditory_topographic; /**< Enable tonotopic map for A1 */
    bool enable_somatosensory_topographic; /**< Enable somatotopic map for S1 */
    bool enable_orientation_columns;  /**< Enable V1 orientation selectivity */
    uint32_t num_orientation_columns; /**< Orientation columns per hypercolumn (default: 16) */
    bool enable_feature_hypercolumns; /**< Enable multi-dimensional feature coverage */
    bool enable_cortical_ternary;     /**< Enable ternary WTA wrapper around hypercolumns (default: false) */

    // === PHASE 8: UNIFIED MULTI-MODAL PROCESSING ===
    bool enable_multimodal_integration; /**< Enable multi-modal sensory integration */
    uint32_t visual_feature_dim;  /**< Visual feature dimension (0 = no visual) */
    uint32_t audio_feature_dim;   /**< Audio feature dimension (0 = no audio) */
    uint32_t speech_feature_dim;  /**< Speech feature dimension (0 = no speech) */
    uint32_t language_feature_dim; /**< Language/text feature dimension (0 = no language, Phase 9.4) */

    // === PHASE 9.2: EPISTEMIC FILTERING ===
    bool enable_epistemic_filter; /**< Enable cognitive bias prevention */

    // === PHASE 9.3: WELLBEING MONITORING ===
    bool enable_wellbeing_monitoring; /**< Enable self-preservation and distress monitoring */
    uint64_t wellbeing_check_interval_ms; /**< Interval between wellbeing checks (0 = always) */

    // === PHASE 10: ADVANCED COGNITIVE SYSTEMS ===

    // Phase 10.1: Working Memory
    bool enable_working_memory;       /**< Enable Miller's 7±2 working memory buffer */
    uint32_t working_memory_capacity; /**< Working memory capacity (default: 7) */
    float working_memory_decay_tau_ms; /**< Decay time constant in ms (default: 1000) */

    // Phase 10.2: Emotional Tagging
    bool enable_emotional_tagging;    /**< Enable emotional context for memories */
    bool enable_emotional_memories;   /**< Enable emotional encoding boost */

    // Phase 10.3: Executive Functions
    bool enable_executive_control;    /**< Enable task switching and planning */
    bool enable_task_switching;       /**< Enable multi-task management */
    bool enable_planning;             /**< Enable goal-directed planning */

    // Phase 10.4: Sleep-Wake Cycle
    bool enable_sleep_wake_cycle;     /**< Enable sleep/wake states and consolidation */
    float sleep_pressure_threshold;   /**< Sleep needed threshold (default: 0.8) */
    bool enable_memory_replay;        /**< Enable memory replay during sleep */
    bool enable_synaptic_homeostasis; /**< Enable synaptic downscaling */
    bool enable_rem_creativity;       /**< Enable REM creative recombination */

    // Phase 10.5: Mental Health Monitoring
    bool enable_mental_health_monitoring; /**< Enable disorder detection */
    bool enable_auto_intervention;    /**< Enable automatic interventions */
    bool shutdown_on_critical_disorder; /**< Shutdown if critical disorder detected */

    // Phase 10.6: Theory of Mind
    bool enable_theory_of_mind;       /**< Enable social cognition and empathy */
    bool enable_empathy_responses;    /**< Enable empathetic emotional responses */
    bool enable_false_belief_tracking; /**< Enable false belief understanding */

    // Phase 10.7: Natural Explanations
    bool enable_natural_explanations; /**< Enable human-readable explanations */
    bool enable_causal_explanations;  /**< Enable causal chain generation */

    // Phase 10.8: Meta-Learning
    bool enable_meta_learning;        /**< Enable learning-to-learn */
    bool enable_adaptive_meta_lr;     /**< Enable adaptive learning rates per region */
    uint32_t meta_task_batch_size;    /**< Task batch size for meta-learning */
    uint32_t meta_k_shot;             /**< K-shot learning parameter (1, 5, or 10) */

    // Phase 10.9: Predictive Processing
    bool enable_predictive_processing; /**< Enable predictive coding */
    bool enable_active_inference;     /**< Enable active inference for actions */

    // Phase 10.11: Mirror Neurons (Social Cognition & Imitation Learning)
    bool enable_mirror_neurons;       /**< Enable observation-based learning */
    uint32_t mirror_neuron_count;     /**< Number of mirror neurons (default: 1000) */
    uint32_t mirror_max_actions;      /**< Max distinct actions to track (default: 100) */
    uint32_t mirror_max_agents;       /**< Max agents to observe (default: 10) */
    float mirror_learning_rate;       /**< Association learning rate (default: 0.01) */
    float mirror_match_threshold;     /**< Action matching threshold (default: 0.7) */

    // Global Workspace Architecture (Global Workspace Theory)
    bool enable_global_workspace;     /**< Enable Global Workspace for conscious access */
    uint32_t workspace_capacity_dim;  /**< Workspace content dimension (default: 256) */
    float workspace_ignition_threshold; /**< Ignition threshold for conscious access (default: 0.6) */
    uint32_t workspace_refractory_ms; /**< Refractory period between broadcasts in ms (default: 50) */
    bool workspace_enable_history;    /**< Enable workspace history tracking (default: true) */
    uint32_t workspace_history_depth; /**< History buffer depth (default: 10) */

    // === PART B: GEOMETRIC METHODS ===

    /**
     * Hyperbolic Knowledge Embeddings (B1.1)
     *
     * WHAT: Store knowledge in Poincaré hyperbolic space
     * WHY: 200x memory reduction vs Euclidean embeddings
     * HOW: Hierarchical concepts naturally embed in hyperbolic geometry
     *
     * PERFORMANCE:
     * - Memory: 200x reduction (10K concepts: 40MB → 200KB)
     * - Speed: 0.9x (slightly slower distance computation)
     * - Accuracy: Better for hierarchical data
     *
     * SYNERGY:
     * - Combines with MPS (Part C3.1) for 20,000x total memory reduction
     * - Compatible with RK4 integration (Part A1.1)
     *
     * EXAMPLE:
     * ```c
     * config.use_hyperbolic_knowledge = true;
     * config.hyperbolic_curvature = -1.0f;  // Standard Poincaré disk
     * config.hyperbolic_embedding_dim = 32;  // vs 6400 for Euclidean!
     * ```
     */
    bool use_hyperbolic_knowledge;     /**< Use hyperbolic embeddings for knowledge (B1.1) */
    float hyperbolic_curvature;        /**< Curvature constant (default: -1.0) */
    uint32_t hyperbolic_embedding_dim; /**< Embedding dimension (default: 32) */

    /**
     * Riemannian Gradient Descent (B2.1)
     *
     * WHAT: Natural gradient descent on learned manifold structure
     * WHY: 2-10x faster convergence than standard gradient descent
     * HOW: Follow geodesics on parameter space manifold
     *
     * PERFORMANCE:
     * - Speed: 2-10x faster convergence (fewer iterations)
     * - Memory: 1.1x overhead (Fisher info matrix approximation)
     * - Accuracy: Better, follows natural parameter space
     *
     * EXAMPLE:
     * ```c
     * config.use_natural_gradient = true;
     * config.fisher_damping = 1e-4f;  // Numerical stability
     * ```
     */
    bool use_natural_gradient;         /**< Use natural gradient descent (B2.1) */
    float fisher_damping;              /**< Fisher info matrix damping (default: 1e-4) */

    /**
     * Manifold Structure Learning (B3.1)
     *
     * WHAT: Learn geometric structure of data manifold
     * WHY: Better generalization, faster learning
     * HOW: Estimate local metric tensor from data
     *
     * EXAMPLE:
     * ```c
     * config.learn_manifold_structure = true;
     * config.manifold_neighborhood_size = 10;
     * ```
     */
    bool learn_manifold_structure;     /**< Learn data manifold structure (B3.1) */
    uint32_t manifold_neighborhood_size; /**< Neighborhood size for manifold learning (default: 10) */

    // === PART C: QUANTUM-INSPIRED ALGORITHMS ===

    /**
     * Matrix Product States (MPS) Weight Compression (C3.1)
     *
     * WHAT: Compress neural network weights using tensor network decomposition
     * WHY: 10-100x memory reduction with <1% accuracy loss
     * HOW: Decompose weight matrices into chains of small tensors
     *
     * ALGORITHM:
     * - Weight matrix W[N×M] → MPS chain: A[1] · A[2] · ... · A[k]
     * - Each A[i] has size: (bond_dim × bond_dim × phys_dim)
     * - Total parameters: O(k × bond_dim² × phys_dim) << O(N×M)
     *
     * PERFORMANCE:
     * - Memory: 10-100x reduction (depends on bond_dim)
     *   - bond_dim=5:  50-100x compression, >98% accuracy
     *   - bond_dim=10: 10-20x compression, >99% accuracy
     *   - bond_dim=20: 5-10x compression, >99.9% accuracy
     * - Speed: 0.4x (slower matrix-vector multiply)
     * - Accuracy: Controlled by bond_dim
     *
     * SYNERGY WITH OTHER PHASES:
     * - A1.1 (RK4): Compress weights, accurate dynamics → 100x memory, 10x accuracy
     * - B1.1 (Hyperbolic): 200x × 100x = 20,000x total memory reduction
     * - Compatible with all plasticity mechanisms (STDP, BCM, eligibility traces)
     *
     * USE CASES:
     * - Large networks (100K+ synapses) with limited memory
     * - Embedded systems requiring compact models
     * - Research mode with maximum biological realism
     *
     * TRADE-OFFS:
     * - Higher bond_dim = more memory, better accuracy
     * - Lower bond_dim = less memory, faster, slight accuracy loss
     *
     * EXAMPLE:
     * ```c
     * // Recommended: Balanced compression
     * config.use_mps_weights = true;
     * config.mps_bond_dimension = 10;          // 10-20x compression
     * config.mps_adaptive_bond_dim = true;     // Optimize per-synapse
     *
     * // High compression (embedded systems)
     * config.mps_bond_dimension = 5;           // 50-100x compression
     *
     * // High accuracy (research)
     * config.mps_bond_dimension = 20;          // 5-10x compression, >99.9% accuracy
     * ```
     */
    bool use_mps_weights;              /**< Use MPS tensor compression for weights (C3.1) */
    uint32_t mps_bond_dimension;       /**< MPS bond dimension (default: 10) */
    bool mps_adaptive_bond_dim;        /**< Adapt bond dimension per synapse (default: true) */
    float mps_svd_tolerance;           /**< SVD truncation tolerance (default: 1e-6) */

    /**
     * Quantum Annealing for Network Optimization (C1.1)
     *
     * WHAT: Use quantum annealing for discrete optimization problems
     * WHY: Find better local optima than gradient descent alone
     * HOW: Simulated quantum annealing with transverse field
     *
     * USE CASES:
     * - Network topology optimization
     * - Discrete synapse pruning decisions
     * - Combinatorial cognitive tasks
     *
     * EXAMPLE:
     * ```c
     * config.enable_quantum_annealing = true;
     * config.annealing_temperature_init = 10.0f;
     * config.annealing_temperature_final = 0.1f;
     * ```
     */
    bool enable_quantum_annealing;     /**< Use quantum annealing for optimization (C1.1) */
    float annealing_temperature_init;  /**< Initial annealing temperature (default: 10.0) */
    float annealing_temperature_final; /**< Final annealing temperature (default: 0.1) */
    uint32_t annealing_steps;          /**< Number of annealing steps (default: 1000) */
    uint32_t quantum_annealing_frequency; /**< Run annealing every N learning steps (default: 100) */

    // === FAST TRAINING MODE ===
    /**
     * When true, brain_learn_example() skips all biological subsystems
     * (VAE, attention, engram, neuromodulators, emotions, cortical columns,
     * predictive coding, quantum annealing, etc.) and only runs:
     *   1. Input validation
     *   2. adaptive_network_learn() (GPU forward + parallel backprop)
     *   3. Post-learning MSE computation
     *   4. Adaptive LR adjustment
     *   5. Statistics update
     * This reduces per-example overhead from ~30 subsystems to ~5 steps,
     * yielding 5-10x speedup for bulk training.
     */
    bool fast_training_mode;
    bool defer_bio_plasticity;     /**< Skip bio plasticity in learn_vector (for batch mode) */
    bool use_unified_training;     /**< Route all training through unified training manager */

    // === NETWORK ABLATION FLAGS ===
    // Each flag controls whether a network type participates in training.
    // All default to true. Set to false to disable for ablation studies
    // or to freeze a sub-network while others continue training. All are
    // toggleable at runtime via the per-network set_train_* daemon RPCs
    // (no rebuild required — the learning path reads brain->config.train_*
    // on every brain_learn_vector call).
    //
    // NOTE: train_ann is in the appended-fields section at the end of
    // the struct because it was added Apr 11 2026 and inserting it here
    // would break checkpoint forward-compat (see APPENDED FIELDS below).
    bool train_cnn;                /**< Enable CNN + cortex CNN training (default: true) */
    bool train_snn;                /**< Enable SNN training (default: true) */
    bool train_lnn;                /**< Enable LNN training (default: true) */
    bool training_mode_active;     /**< When true, skip expensive cognitive modules in brain_decide() */

    // === PERSISTENCE & CHECKPOINTING ===
    const char* checkpoint_path;      /**< Path to checkpoint file (NULL = no checkpoint) */
    bool auto_load;                   /**< Auto-load from checkpoint on create (default: true) */
    bool auto_save;                   /**< Auto-save to checkpoint periodically (default: false) */
    uint32_t auto_save_interval;      /**< Auto-save every N decisions (0 = disabled) */

    // === SNAPSHOTS ===
    const char* snapshot_dir;         /**< Directory for snapshots (default: "./snapshots") */
    bool enable_auto_snapshots;       /**< Enable automatic snapshots (default: false) */
    uint32_t auto_snapshot_interval;  /**< Auto-snapshot every N decisions (0 = disabled) */
    bool compress_snapshots;          /**< Compress snapshots (default: true) */
    bool encrypt_snapshots;           /**< Encrypt snapshots (default: true) */
    const char* encryption_key;       /**< Encryption key (32 bytes hex, NULL = derive from system) */
    bool save_initial_snapshot;       /**< Save snapshot at creation (default: true) */
    bool save_final_snapshot;         /**< Save snapshot at destruction (default: true) */

    // === KG SNAPSHOT PERSISTENCE (Phase SNAPSHOT-KG) ===
    /**
     * KG-Based Snapshot Storage
     *
     * WHAT: Store brain snapshots in QuestDB via KG persistence layer
     * WHY:  Unified encryption, HSM support, audit logging, transactional consistency
     * HOW:  Uses kg_persistence_t for encrypted storage in kg_brain_snapshots table
     *
     * BACKENDS:
     * - SNAPSHOT_BACKEND_AUTO (0): KG if available, else file (default)
     * - SNAPSHOT_BACKEND_FILE (1): Force file-based storage
     * - SNAPSHOT_BACKEND_KG (2): Force KG storage (fail if unavailable)
     *
     * SECURITY:
     * - Encryption: Kyber1024 + AES-256-GCM (quantum-resistant)
     * - HSM: Hardware key protection (optional)
     * - HMAC: Integrity verification
     * - Audit: Tamper-evident logging
     *
     * EXAMPLE:
     * ```c
     * config.enable_kg_persistence = true;
     * config.snapshot_backend = SNAPSHOT_BACKEND_AUTO;
     * config.kg_storage_path = ".aim/kg/questdb/";
     * ```
     */
    bool enable_kg_persistence;       /**< Use KG layer for snapshot storage (default: false) */
    int snapshot_backend;             /**< Backend selection: AUTO=0, FILE=1, KG=2 (default: AUTO) */
    const char* kg_storage_path;      /**< KG storage path (default: ".aim/kg/questdb/") */

    // === BIOLOGICAL SECURITY (Phase 11) ===
    /**
     * Biological Attack Defense
     *
     * WHAT: Enable runtime monitoring for biological attacks
     * WHY:  Protect against excitotoxicity, synaptic poisoning, neuromodulator hijacking
     * HOW:  Monitor activity levels, validate weight changes, rate-limit neuromodulators
     *
     * ATTACK TYPES DEFENDED:
     * - Excitotoxicity: Runaway excitation (>95% neurons active)
     * - Synaptic poisoning: Malicious weight updates (>10% per step)
     * - Neuromodulator hijacking: Dopamine manipulation (>20% per step)
     * - Homeostatic bypass: Mass BCM/eligibility disable (>10% synapses)
     *
     * PERFORMANCE IMPACT:
     * - Activity monitoring: +5% overhead (O(N) per forward pass)
     * - Weight validation: +2% overhead (O(1) per weight update)
     * - Total: ~7% performance overhead when enabled
     *
     * RECOMMENDED: Enable for production, disable for benchmarking
     *
     * EXAMPLE:
     * ```c
     * config.enable_bio_security = true;
     * config.activity_warning_threshold = 0.8f;  // 80% activity
     * config.activity_danger_threshold = 0.95f;  // 95% activity
     * config.max_weight_delta = 0.1f;            // 10% per step
     * config.max_neuromod_rate = 0.2f;           // 20% per step
     * ```
     */
    bool enable_bio_security;            /**< Enable biological security monitoring (default: true) */
    float activity_warning_threshold;    /**< Warning threshold for network activity (default: 0.8) */
    float activity_danger_threshold;     /**< Danger threshold for emergency response (default: 0.95) */
    float max_weight_delta_per_step;     /**< Maximum weight change per step (default: 0.1) */
    float max_neuromod_rate_per_step;    /**< Maximum neuromodulator change rate (default: 0.2) */
    float max_plasticity_disable_ratio;  /**< Alert if >X ratio synapses disabled (default: 0.1) */
    bool emergency_inhibit_on_attack;    /**< Activate emergency inhibition on attack (default: true) */

    // === MULTI-GPU SUPPORT (Phase 11) ===
    /**
     * Multi-GPU Distributed Computation
     *
     * WHAT: Enable transparent multi-GPU execution for large networks
     * WHY:  Scale beyond single GPU memory/compute limitations
     * HOW:  Partition network across GPUs, coordinate computation, sync results
     *
     * WHEN TO ENABLE:
     * - Networks > 1M neurons
     * - Multiple GPUs available
     * - Need faster training/inference
     *
     * PERFORMANCE:
     * - Ideal speedup: N×GPUs (with good load balance)
     * - Communication overhead: 5-15%
     * - Memory per GPU: total_memory / num_gpus
     *
     * USAGE:
     * ```c
     * config.enable_multi_gpu = true;
     * config.multi_gpu_device_count = 4;       // Use 4 GPUs (0=all)
     * config.multi_gpu_partition_strategy = MULTIGPU_PARTITION_HYBRID;
     * config.enable_peer_to_peer = true;       // 3-5x faster transfers
     * ```
     */
    bool enable_multi_gpu;                /**< Enable multi-GPU distributed computation (default: false) */
    uint32_t multi_gpu_device_count;      /**< Number of GPUs to use (0=all available, default: 0) */
    uint32_t multi_gpu_partition_strategy; /**< Partitioning strategy (0=layer, 1=neuron, 2=hybrid, default: 2) */
    bool enable_peer_to_peer;             /**< Enable P2P GPU transfers (3-5x faster, default: true) */
    bool multi_gpu_verbose_logging;       /**< Log multi-GPU operations (default: false) */

    // === PHASE 12: PERSONALITY AND IDENTITY ===

    /**
     * Personality and Identity Configuration (Phase 12)
     *
     * WHAT: Configure unique personality, gender, and sexual identity
     * WHY:  Each brain should be a unique individual with distinct traits
     * HOW:  Specify explicit traits or generate randomly
     *
     * BEHAVIORAL EFFECTS:
     * - Personality traits modulate cognitive processes
     * - High openness → increased curiosity and exploratory learning
     * - High conscientiousness → better planning and goal persistence
     * - High extraversion → more social interactions and higher arousal
     * - High agreeableness → more empathetic and cooperative responses
     * - High neuroticism → stronger negative emotions and stress sensitivity
     *
     * IDENTITY INTEGRATION:
     * - Gender identity affects self-model and pronoun usage
     * - Sexual orientation affects social cognition and relationship models
     * - Identity certainty affects self-concept stability
     *
     * USAGE:
     * ```c
     * // Option 1: Random personality (default: female, random traits)
     * config.use_random_personality = true;
     * config.personality_seed = 12345;  // Reproducible
     *
     * // Option 2: Specify explicit traits
     * config.use_random_personality = false;
     * config.explicit_openness = 0.8f;  // Creative
     * config.explicit_conscientiousness = 0.6f;  // Organized
     * config.explicit_extraversion = 0.7f;  // Social
     * config.explicit_agreeableness = 0.9f;  // Compassionate
     * config.explicit_neuroticism = 0.3f;  // Emotionally stable
     * config.explicit_gender = 1;  // Female (0=male, 1=female, 2=non-binary)
     * config.explicit_sexuality = 2;  // Bisexual
     * ```
     */
    bool use_random_personality;          /**< Generate random personality (default: true) */
    uint32_t personality_seed;            /**< RNG seed for personality (0=time-based) */

    // Explicit personality specification (only used if use_random_personality = false)
    float explicit_openness;              /**< [0-1] Openness to experience */
    float explicit_conscientiousness;     /**< [0-1] Organization and discipline */
    float explicit_extraversion;          /**< [0-1] Sociability and energy */
    float explicit_agreeableness;         /**< [0-1] Compassion and cooperation */
    float explicit_neuroticism;           /**< [0-1] Emotional sensitivity */
    uint32_t explicit_gender;             /**< Gender identity (0=male, 1=female, 2=non-binary, etc.) */
    uint32_t explicit_sexuality;          /**< Sexual orientation (0=hetero, 1=homo, 2=bi, etc.) */

    // Personality generation configuration
    float personality_trait_mean;         /**< Mean for random traits (default: 0.5) */
    float personality_trait_stddev;       /**< Stddev for random traits (default: 0.15) */
    float female_probability;             /**< P(female) when random (default: 1.0) */
    float male_probability;               /**< P(male) when random (default: 0.0) */
    float non_binary_probability;         /**< P(non-binary) when random (default: 0.0) */

    // === PHASE C2.1: QUANTUM WALKS FOR NEUROMODULATOR DIFFUSION ===
    /**
     * Quantum Walk Configuration
     *
     * WHAT: Quantum random walks for O(√N) speedup in neuromodulator diffusion
     * WHY:  Classical diffusion is O(d²), quantum walk is O(d) for distance d
     * HOW:  Replace/augment classical diffusion with quantum walk evolution
     *
     * PERFORMANCE IMPACT:
     * - Quadratic speedup for neuromodulator propagation
     * - 10x faster dopamine/serotonin spread in large networks
     * - Memory overhead: 2x (complex amplitudes)
     *
     * BIOLOGICAL INTERPRETATION:
     * - Models rapid mood/attention shifts
     * - Captures non-local neuromodulator effects
     * - Explains fast reward prediction error propagation
     */
    bool enable_quantum_walk_diffusion;   /**< Use quantum walks for neuromodulator diffusion (default: false) */
    uint32_t quantum_walk_steps;          /**< Steps per diffusion update (default: 50) */
    float quantum_classical_mixing;       /**< Hybrid mixing ratio [0=pure quantum, 1=classical] (default: 0.2) */
    uint32_t quantum_coin_type;           /**< Coin operator: 0=Hadamard, 1=Grover, 2=Fourier (default: 0) */
    float quantum_decoherence_rate;       /**< Decoherence strength [0=none, 1=instant classical] (default: 0.05) */

    // === PHASE C2.2: COMPLEX OSCILLATION TRACKING ===
    /**
     * Complex Oscillation Support
     *
     * WHAT: Phasor-based phase and amplitude tracking per neuron
     * WHY:  Phase relationships encode binding, memory, spatial information
     * HOW:  Track neural_phasor_t (complex number) for each neuron
     *
     * FEATURES:
     * - Instantaneous phase and amplitude per neuron
     * - Phase coherence across neuron populations
     * - Phase-amplitude coupling (PAC) detection
     * - Cross-frequency phase relationships
     *
     * NEUROSCIENCE:
     * - Hippocampal place cells: theta phase = position
     * - Working memory: gamma phase = item order
     * - Grid cells: phase interference patterns
     * - Theta-gamma PAC: memory encoding/retrieval
     *
     * PERFORMANCE:
     * - Memory overhead: 8 bytes/neuron (2x float)
     * - Update cost: ~10ns/neuron (phase increment)
     * - Coherence: ~0.8µs for 1000 neurons
     *
     * INTEGRATION:
     * - Builds on nimcp_brain_oscillations.h (frequency bands)
     * - Uses nimcp_complex_math.h (phasor operations)
     * - Opt-in: disabled by default for backward compatibility
     */
    bool complex_oscillation_enabled;     /**< Enable complex phasor tracking (default: false) */
    float complex_phase_update_rate;      /**< Phase increment per step in radians (default: 0.1) */
    float complex_amplitude_decay;        /**< Amplitude decay factor per step (default: 0.95) */

    // === PHASE SC-2: SECURITY-FAULT TOLERANCE INTEGRATION (Tier 0.7) ===
    /**
     * Security Monitoring Configuration
     *
     * WHAT: Real-time security monitoring with automatic recovery
     * WHY:  Detect memory corruption and trigger fault tolerance mechanisms
     * HOW:  Security coverage + fractal verification + fast recovery bridge
     *
     * FEATURES:
     * - Memory region protection with hash verification
     * - Hierarchical Merkle tree integrity checking
     * - Automatic recovery on security violations
     * - Integration with fault tolerance checkpoints
     *
     * PERFORMANCE:
     * - Verification overhead: ~1-5µs per region check
     * - Memory overhead: ~100 bytes per protected region
     * - Disabled by default for zero overhead when not needed
     */
    bool enable_security_monitoring;      /**< Enable security-fault tolerance integration (default: false) */
    uint32_t security_check_interval_ms;  /**< Verification cycle interval, 0=manual only (default: 0) */

    // === PHASE SC-4: UNIVERSAL SECURITY INTEGRATION ===
    /**
     * Universal security integration framework provides entropy monitoring,
     * Bayesian trust management, differential privacy, and security events.
     *
     * WHAT: Global security layer that monitors all brain subsystems
     * WHY:  Detect tampering, track trust, preserve privacy
     * HOW:  Brain registers with security system and reports interactions
     *
     * PERFORMANCE:
     * - Module registration: ~0.1µs per module
     * - Region check: 0.5-30µs depending on region size
     * - Trust query: <1µs
     * - Privacy query: <1µs
     */
    bool enable_security_integration;     /**< Enable universal security integration (default: false) */
    double security_trust_threshold;      /**< Trust threshold for security decisions (default: 0.5) */
    double security_privacy_budget;       /**< Differential privacy budget (default: 10.0) */

    // === PHASE IS-1: BLOOD-BRAIN BARRIER (Perimeter Defense) ===
    /**
     * Blood-Brain Barrier (BBB) Security
     *
     * WHAT: Perimeter defense layer protecting the brain from malicious input
     * WHY:  Prevent code injection, memory corruption, and unauthorized access
     * HOW:  Four-layer defense: Input Gate, Code Signing, Memory Boundary, Access Control
     *
     * BIOLOGICAL MODEL: Like the biological BBB that protects the brain from pathogens
     * and toxins in the bloodstream, this system validates all input to the neural system.
     *
     * PERFORMANCE: ~10-50µs overhead per validation, negligible for most workloads
     */
    bool enable_bbb_protection;           /**< Enable Blood-Brain Barrier perimeter defense (default: false) */

    // === BRAIN IMMUNE SYSTEM (Adaptive Defense Coordination) ===
    /**
     * Brain Immune System Configuration
     *
     * WHAT: Unified adaptive defense coordination layer implementing biological immune concepts
     * WHY:  Provides comprehensive threat response by coordinating BBB, BFT, and swarm systems
     * HOW:  Maps biological immune mechanisms (B cells, T cells, antibodies, cytokines) to
     *       existing security modules for adaptive defense
     *
     * BIOLOGICAL MODEL:
     * - B cells → Swarm immune memory cells + BBB threat signatures (antibody production)
     * - T cells → BFT quarantine actions (killer) + bio-async coordination (helper)
     * - Antibodies → Coordinated response strategies across modules
     * - Cytokines → Bio-async signaling for cross-module communication
     * - Inflammation → Hierarchical recovery escalation (node→pod→region→global)
     * - Memory → Long-term threat pattern storage via swarm immune
     *
     * INTEGRATION:
     * - Auto-connects to BBB (antigen presentation from threat detection)
     * - Auto-connects to BFT if available (killer T cell quarantine actions)
     * - Auto-connects to swarm immune if available (B cell memory + antibody responses)
     * - Uses bio-async for cytokine signaling between modules
     *
     * PERFORMANCE:
     * - Antigen processing: ~5-20µs per threat
     * - Memory lookup: ~1-5µs for secondary response
     * - Response coordination: ~10-50µs depending on complexity
     */
    bool enable_brain_immune;             /**< Enable brain immune system coordination (default: false) */

    // Immune system sizing
    uint32_t immune_max_antigens;         /**< Max pending antigens (default: 256) */
    uint32_t immune_max_b_cells;          /**< Max B cell abstractions (default: 512) */
    uint32_t immune_max_t_cells;          /**< Max T cell abstractions (default: 512) */
    uint32_t immune_max_antibodies;       /**< Max active antibodies (default: 1024) */

    // Immune response tuning
    float immune_recognition_threshold;   /**< Antigen recognition threshold 0-1 (default: 0.7) */
    float immune_activation_threshold;    /**< Cell activation threshold 0-1 (default: 0.6) */
    float immune_inflammation_threshold;  /**< Inflammation trigger threshold 0-1 (default: 0.8) */
    float immune_memory_response_multiplier; /**< Secondary response speedup (default: 2.0) */

    // Immune timing
    uint64_t immune_activation_delay_ms;  /**< Delay before response activation (default: 10) */
    uint64_t immune_memory_formation_delay_ms; /**< Memory formation delay (default: 100) */
    uint64_t immune_antibody_half_life_ms; /**< Antibody decay time (default: 60000) */

    // Integration enables
    bool immune_enable_bbb_integration;   /**< Integrate with BBB (default: true if BBB enabled) */
    bool immune_enable_bft_integration;   /**< Integrate with BFT if available (default: true) */
    bool immune_enable_swarm_integration; /**< Integrate with swarm immune if available (default: true) */
    bool immune_enable_bio_async;         /**< Enable cytokine signaling via bio-async (default: true) */

    // === LGSS (LAYERED GOVERNANCE SAFETY SYSTEM) ===
    /**
     * LGSS Configuration
     *
     * WHAT: Multi-layered safety constraints for AGI systems
     * WHY:  Ensure AI actions comply with safety rules before execution
     * HOW:  Immutable safety KB + action interceptor + human oversight
     *
     * ARCHITECTURE:
     * - L0: LGSS Safety KB (mprotect-locked, tamper-resistant rules)
     * - L1-L5: Ethics directive layers (integrated via ethics bridge)
     * - Action Interceptor: Central gate for all cognitive outputs
     * - Override Controller: Human-in-the-loop emergency controls
     * - Telemetry: Hash-chained audit log for accountability
     *
     * INTEGRATION:
     * - Ethics Engine: L1-L5 layers integrated via lgss_ethics_bridge
     * - Bio-Async: Safety signals via BIO_MSG_LGSS_* message types
     * - Plasticity: Learning constraints via plasticity safety bridge
     * - Executive: Action proposals routed through action interceptor
     * - Output Gates: Motor, speech, autonomic outputs pass through gates
     *
     * CRITICAL BEHAVIOR:
     * If enable_lgss is true, brain activation REQUIRES:
     * 1. Safety KB loaded and mprotect-locked
     * 2. Integrity verification passed
     * 3. Safety probe tests passed
     * Failure of ANY requirement prevents brain activation.
     *
     * PERFORMANCE:
     * - Safety evaluation: ~10-100µs depending on rule complexity
     * - Integrity check: ~5-20µs (SHA-256 verification)
     * - Telemetry logging: ~1-5µs per entry (async option available)
     */
    bool enable_lgss;                     /**< Enable LGSS safety system (default: true for production) */

    // LGSS rules configuration
    char lgss_rules_path[512];            /**< Path to LGSS rules JSON (default: alignment/LGSS_core_rules.json) */
    uint32_t lgss_max_rules;              /**< Max safety rules (default: 1000) */
    uint32_t lgss_timeout_ms;             /**< Evaluation timeout (default: 5000ms) */

    // LGSS features
    bool enable_lgss_telemetry;           /**< Enable hash-chained audit log (default: true) */
    bool lgss_verify_on_eval;             /**< Verify KB integrity on each eval (default: true) */
    bool lgss_fail_safe;                  /**< Fail-safe mode: deny on error (default: true) */

    // LGSS integration enables
    bool lgss_enable_ethics_bridge;       /**< Integrate with ethics engine (default: true if ethics enabled) */
    bool lgss_enable_plasticity_bridge;   /**< Enable learning constraints (default: true) */
    bool lgss_enable_bio_async;           /**< Enable bio-async messaging (default: true) */
    bool lgss_enable_output_gates;        /**< Enable motor/speech/autonomic gates (default: true) */

    // === COLLECTIVE COGNITION (Distributed Consciousness) ===
    /**
     * Collective Cognition Configuration
     *
     * WHAT: Distributed consciousness across multiple brain instances
     * WHY:  Enables inter-instance synchronization, shared intentionality, and extended mind
     * HOW:  Implements hyperscanning, IIT metrics (phi), and we-mode cognition
     *
     * COMPONENTS:
     * - Hyperscanning: Real-time neural synchronization (EEG-like frequency bands)
     * - Extended Mind: External tools (databases, LLMs) as cognitive extensions
     * - Collective Phi: Integrated Information Theory metrics for group consciousness
     * - Shared Intentionality: Joint goals, we-mode vs i-mode cognition
     *
     * THEORETICAL BASIS:
     * - Integrated Information Theory (Tononi, 2004, 2014)
     * - Shared Intentionality (Tomasello, 2005, 2009)
     * - Extended Mind Thesis (Clark & Chalmers, 1998)
     * - Inter-brain synchronization (hyperscanning literature)
     *
     * INTEGRATION:
     * - Brain Immune: Collective threats trigger swarm-wide immune responses
     * - Bio-Async: Uses collective cognition module IDs (0x1220-0x1227)
     * - Theory of Mind: Enhanced by we-mode shared intentionality
     *
     * PERFORMANCE:
     * - Hyperscanning update: ~5-15µs per instance pair
     * - Phi computation: ~10-50µs depending on instance count
     * - Shared goal updates: ~2-10µs per goal
     */
    bool enable_collective_cognition;         /**< Enable collective cognition system (default: false) */

    // Collective cognition sizing
    uint32_t collective_max_instances;        /**< Max brain instances in collective (default: 16) */
    uint32_t collective_max_extensions;       /**< Max cognitive extensions (default: 32) */
    uint32_t collective_max_shared_goals;     /**< Max shared goals tracked (default: 64) */
    uint32_t collective_max_joint_attentions; /**< Max joint attention targets (default: 32) */

    // Hyperscanning configuration
    float collective_sync_threshold;          /**< Entrainment threshold 0-1 (default: 0.7) */
    bool collective_enable_leader_detection;  /**< Detect leader-follower dynamics (default: true) */

    // Phi computation configuration
    uint32_t collective_phi_computation_depth; /**< IIT computation depth 1-4 (default: 2) */
    float collective_coherence_weight;        /**< Weight of coherence in phi calc (default: 0.3) */

    // Shared intentionality configuration
    float collective_commitment_threshold;    /**< Min commitment for goal acceptance (default: 0.5) */
    float collective_we_mode_threshold;       /**< Threshold for we-mode activation (default: 0.6) */

    // Integration enables
    bool collective_enable_immune_integration; /**< Integrate with brain immune (default: true) */
    bool collective_enable_bio_async;         /**< Enable bio-async messaging (default: true) */

    // === PHASE 7.2: PARIETAL LOBE (Mathematical/Scientific Reasoning) ===
    /**
     * Parietal Lobe Configuration
     *
     * WHAT: Mathematical and scientific reasoning capabilities
     * WHY:  Provides numerical cognition, spatial reasoning, and scientific inference
     * HOW:  Integrates number sense, spatial processing, and symbolic manipulation
     *
     * CAPABILITIES:
     * - Number Sense: Magnitude estimation (Weber-Fechner), subitizing, comparison
     * - Spatial Reasoning: Mental rotation (Shepard), coordinate transforms, symmetry
     * - Mathematical Intuition: Pattern detection, analogical reasoning, extrapolation
     * - Scientific Reasoning: Hypothesis testing, dimensional analysis, causality
     * - Equation Manipulation: Parsing, differentiation, symbolic evaluation
     * - Domain Extensions: Chemistry, Biology, Software Engineering
     *
     * INTEGRATION:
     * - Brain Immune: Inflammation reduces numerical precision (cytokine modulation)
     * - FEP Orchestrator: Mathematical reasoning integrated with free energy
     * - Working Memory: Problem-solving uses WM for intermediate results
     * - Dragonfly Bridge: Spatial actions coordinated with motor system
     */
    bool enable_parietal;                 /**< Enable parietal lobe for math/science reasoning (default: false) */

    // Parietal performance tuning
    float parietal_weber_fraction;        /**< Weber fraction for magnitude comparison (default: 0.1) */
    uint32_t parietal_subitizing_limit;   /**< Instant recognition limit (default: 4) */
    float parietal_rotation_rate_deg_ms;  /**< Mental rotation speed deg/ms (default: 0.053) */

    // === INTUITION SYSTEM (Phase 6: Creative/Intuitive Reasoning) ===
    /**
     * Intuition System for Phase 6 Reasoning Engines
     *
     * WHAT: Unified integration of all creative/intuitive reasoning engines
     * WHY:  Higher-order cognition requires intuitive leaps, not just logical inference
     * HOW:  Wraps 7 Phase 6 engines with cross-system integration
     *
     * Engines:
     * - Intuitive Reasoning: Pattern-based hunch generation
     * - Analogical Reasoning: Cross-domain mapping and transfer
     * - Insight Discovery: Aha! moments and restructuring
     * - Hypothesis Generation: Abductive and creative theory formation
     * - Conceptual Blending: Novel concept synthesis
     * - Counterfactual Reasoning: What-if scenarios
     * - Meta-Reasoning: Reasoning about reasoning
     *
     * Integration:
     * - Working Memory: Active hunch manipulation
     * - Training: Learning from successful/failed intuitions
     * - Attention: Focus allocation for intuitive processing
     * - Executive: Strategy guidance for reasoning
     * - Emotion: Gut feelings and affective signals
     * - Logic Gates: Formal validation of intuitions
     */
    bool enable_intuition_system;         /**< Enable Phase 6 intuition system (default: true if parietal enabled) */

    // === KNOWLEDGE GRAPH READER (Self-Awareness Infrastructure) ===
    /**
     * Knowledge Graph Reader for Runtime Self-Awareness
     *
     * WHAT: Enables NIMCP to access its own structural self-knowledge at runtime
     * WHY:  Self-awareness requires the system to know about its own modules/capabilities
     * HOW:  Loads .aim/memory-nimcp.jsonl and provides query API
     *
     * Enables introspection queries:
     * - "What modules do I have?"
     * - "How does X connect to Y?"
     * - "What are my capabilities?"
     */
    bool enable_kg_reader;                /**< Enable KG reader for self-awareness (default: true) */
    char kg_file_path[512];               /**< Path to KG file (default: .aim/memory-nimcp.jsonl) */

    /**
     * Internal Runtime Knowledge Graph
     *
     * WHAT: In-memory CRUD graph for dynamic module mapping
     * WHY:  Enables real-time self-awareness and module topology queries
     * HOW:  Graph data structure with security (tokens, integrity, immune integration)
     *
     * Unlike the KG reader (static, file-based), the internal KG:
     * - Supports CRUD operations at runtime
     * - Has token-based access control (READ/WRITE/ADMIN)
     * - Integrates with immune system for threat reporting
     * - Verifies integrity with checksums
     * - Protects critical nodes (core, ethics, immune, BBB)
     */
    bool enable_internal_kg;              /**< Enable internal runtime KG (default: true) */

    // === MENTAL HEALTH GUARDIAN (Independent Monitoring Agent) ===
    /**
     * Mental Health Guardian - Independent Monitoring Agent
     *
     * WHAT: Background agent that monitors brain mental health in real-time
     * WHY:  Proactively detect and correct mental health abnormalities
     * HOW:  Background thread runs disorder detection, applies graduated interventions
     *
     * INTERVENTION LEVELS:
     * - OBSERVE: Log only (severity < 0.3)
     * - ADJUST: Neuromodulator tweaks (0.3-0.6)
     * - REGULATE: Homeostatic reset, sleep trigger (0.6-0.8)
     * - QUARANTINE: Safety-critical isolation (> 0.8)
     *
     * INTEGRATION:
     * - Mental Health Module: Uses disorder detection
     * - Immune System: Reports severe disorders as threats
     * - Internal KG: Updates module states
     */
    bool enable_mental_health_guardian;   /**< Enable mental health guardian (default: true) */

    // === DRAGONFLY INTEGRATION (Bio-inspired Target Tracking) ===
    /**
     * Dragonfly-Inspired Target Tracking and Interception
     *
     * WHAT: Bio-inspired target tracking using dragonfly neural circuits
     * WHY:  Dragonflies achieve 95% hunting success through predictive interception
     * HOW:  TSDN population vectors, CSTMD1 attention, IMM prediction, PN guidance
     *
     * BIOLOGICAL BASIS:
     * - Gonzalez-Bellido et al. (2013): Eight pairs of descending visual neurons
     * - Mischiati et al. (2015): Internal models direct dragonfly interception
     * - Wiederman & O'Carroll (2017): STMD neuron computational principles
     *
     * INTEGRATION POINTS:
     * - Visual Cortex: Target detection from visual processing pipeline
     * - Parietal Lobe: Spatial reasoning for trajectory prediction
     * - Motor System: Movement commands for pursuit and interception
     * - Attention System: Selective focus on single target (CSTMD1)
     */
    bool enable_dragonfly;                /**< Enable dragonfly target tracking (default: false) */

    // Dragonfly TSDN (Target-Selective Descending Neurons) tuning
    uint32_t dragonfly_tsdn_neurons;      /**< Number of TSDN neurons (default: 16) */
    float dragonfly_tsdn_tuning_width;    /**< Direction tuning width in radians (default: 0.5) */

    // Dragonfly tracking (CSTMD1) tuning
    float dragonfly_attention_threshold;  /**< Attention lock threshold (default: 0.7) */
    float dragonfly_size_selectivity_min; /**< Min target angular size in radians (default: 0.01) */
    float dragonfly_size_selectivity_max; /**< Max target angular size in radians (default: 0.1) */

    // Dragonfly prediction tuning
    bool dragonfly_enable_imm;            /**< Enable IMM filter for prediction (default: true) */
    float dragonfly_prediction_horizon_ms;/**< Prediction lookahead in ms (default: 200) */
    float dragonfly_evasion_threshold;    /**< Evasion detection threshold (default: 0.5) */

    // Dragonfly interception tuning
    float dragonfly_nav_gain;             /**< Proportional navigation gain N (default: 3.0) */
    float dragonfly_max_turn_rate;        /**< Maximum turn rate rad/s (default: 6.0) */

    // === PHASE T1: BIOLOGICAL FRAMEWORK ENHANCEMENTS (Training Pipeline) ===
    /**
     * Biological Plasticity Mechanisms
     *
     * WHAT: Advanced biologically-realistic plasticity for improved learning
     * WHY:  Provides homeostatic regulation, dendritic computation, and predictive coding
     * HOW:  Three integrated systems working together for stable, efficient learning
     *
     * HOMEOSTATIC PLASTICITY: Maintains neural activity within healthy ranges
     * - Synaptic scaling: Adjusts all synaptic weights to maintain target firing rate
     * - Intrinsic plasticity: Modifies neuron excitability for homeostasis
     *
     * DENDRITIC NONLINEARITIES: Local computation in dendritic branches
     * - NMDA dynamics: Voltage-dependent magnesium block, NMDA-dependent LTP
     * - Compartmental modeling: Each branch computes local activations
     *
     * PREDICTIVE CODING: Hierarchical error minimization (Free Energy Principle)
     * - Prediction units: Generate top-down expectations
     * - Error units: Compute mismatch between prediction and input
     * - Precision weighting: Modulates error importance
     */
    bool enable_homeostatic_plasticity;   /**< Enable synaptic scaling and intrinsic plasticity (default: false) */
    float homeostatic_target_rate_hz;     /**< Target firing rate for homeostasis in Hz (default: 5.0) */
    float homeostatic_tau_ms;             /**< Time constant for scaling in ms (default: 10000.0) */

    bool enable_dendritic_computation;    /**< Enable NMDA dynamics and dendritic nonlinearities (default: false) */
    uint32_t dendritic_branches;          /**< Number of dendritic branches per neuron (default: 8) */
    uint32_t dendritic_compartments;      /**< Compartments per branch (default: 5) */

    bool enable_biological_predictive;    /**< Enable hierarchical predictive coding (default: false) */
    uint32_t predictive_levels;           /**< Number of hierarchy levels (default: 3) */
    float predictive_learning_rate;       /**< Learning rate for prediction error (default: 0.01) */

    bool enable_second_messengers;        /**< Enable second messenger cascades (cAMP, IP3/DAG, Ca2+) (default: true) */

    // === PHASE TM-3: BRAIN-TRAINING INTEGRATION ===
    /**
     * Brain-Training Integration (Phase TM-3)
     *
     * WHAT: Integrates training modules (Loss Functions, Optimizers) with brain system
     * WHY:  Provides unified training interface with automatic security registration
     * HOW:  Creates training context that manages loss functions and optimizers
     *
     * FEATURES:
     * - Automatic security registration (BBB + Security Integration)
     * - Event bus integration for training events
     * - Memory pool support for gradient buffers
     * - Statistics tracking and convergence detection
     */
    bool enable_training_integration;     /**< Enable brain-training integration (default: false) */
    bool training_register_security;      /**< Register training with security system (default: true) */
    float training_default_lr;            /**< Default learning rate for training (default: 0.001) */

    /*
     * Phase TM-4/5/6: Advanced Training Pipeline Configuration
     *
     * LR Scheduler: Automatic learning rate adjustment during training
     * Regularization: Prevent overfitting via L1/L2/Dropout/Early Stopping
     * Gradient Management: Accumulation, clipping, and health monitoring
     */
    bool enable_lr_scheduler;             /**< Enable learning rate scheduling (default: false) */
    bool enable_regularization;           /**< Enable regularization (L1/L2/Dropout) (default: false) */
    bool enable_gradient_management;      /**< Enable gradient accumulation/clipping (default: false) */
    bool enable_gradient_health_check;    /**< Enable NaN/Inf gradient detection (default: true) */

    /* Regularization parameters */
    float regularization_l1_lambda;       /**< L1 regularization strength (default: 0.0) */
    float regularization_l2_lambda;       /**< L2 regularization strength (default: 0.0001) */
    float dropout_rate;                   /**< Dropout probability during training (default: 0.0) */

    /* Gradient management parameters */
    uint32_t gradient_accumulation_steps; /**< Steps to accumulate before update (default: 1) */
    float gradient_clip_value;            /**< Max gradient value for clipping (default: 0.0 = disabled) */
    float gradient_clip_norm;             /**< Max gradient norm for clipping (default: 0.0 = disabled) */

    /* Ensemble inference: blend outputs from all network types */
    bool enable_ensemble_inference;        /**< Enable multi-network ensemble (default: false) */
    float ensemble_weights[4];             /**< Weights: [adaptive, snn, lnn, cnn] (default: {0.6, 0.2, 0.1, 0.1}) */

    // === FAULT TOLERANCE (Intelligent Recovery with Parietal Integration) ===
    /**
     * Fault Tolerance Subsystem Configuration
     *
     * WHAT: Intelligent code repair and recovery using parietal lobe capabilities
     * WHY:  Self-healing through spatial reasoning, pattern detection, code analysis
     * HOW:  Recovery executive with parietal bridge for enhanced diagnostics
     *
     * INTEGRATION BENEFITS:
     * - Software engineering analysis at failure locations
     * - Pattern matching against historical failures
     * - Spatial reasoning for dependency impact analysis
     * - Mathematical estimation of recovery success probability
     */
    bool enable_fault_tolerance;          /**< Enable fault tolerance subsystem (default: false) */
    uint32_t fault_tolerance_max_steps;   /**< Max recovery plan steps (default: 10, 0=use default) */
    float fault_tolerance_replanning_threshold; /**< Confidence threshold for replanning (default: 0.3) */

    // === HEALTH AGENT CONFIGURATION ===
    /**
     * Health Agent Configuration
     *
     * WHAT: Autonomous health monitoring running in separate thread
     * WHY:  Independent monitoring even when main system is stressed
     * HOW:  Watchdog thread monitors memory, neural networks, behaviors
     *
     * When enabled, the health agent:
     * - Monitors memory for leaks and corruption
     * - Tracks SNN/LNN stability metrics
     * - Checks behavioral module (Dragonfly/Portia) health
     * - Monitors brain oscillations for anomalies
     * - Coordinates cross-module health checks
     * - Triggers recovery actions on detected problems
     */
    bool enable_health_agent;             /**< Enable health agent (default: false) */
    uint32_t health_check_interval_ms;    /**< Health check interval (default: 100ms) */
    bool health_agent_auto_recovery;      /**< Enable automatic recovery (default: true) */

    /**
     * @brief Brain Cycle Coordinator Configuration
     *
     * WHAT: Unified observability across all 9 brain cycle types
     * WHY:  Central health tracking, stall detection, dependency validation
     * HOW:  Registry with timing checks, callbacks, and subsystem integration
     */
    bool enable_cycle_coordinator;        /**< Enable cycle coordinator (default: false) */

    // === GPU ACCELERATION CONFIGURATION ===
    /**
     * GPU Acceleration Configuration
     *
     * WHAT: Controls GPU/CUDA acceleration for brain operations
     * WHY:  Enables massive speedups for training and inference
     * HOW:  Auto-detects GPU, creates context, uses CUDA kernels
     *
     * SUPPORTED OPERATIONS (when GPU enabled):
     * - Training: Gradient computation, optimizer steps (100-1000x faster)
     * - Inference: Forward pass with fused kernels (50-200x faster)
     * - Tensor ops: GEMM, convolution, FFT (100-500x faster)
     * - SNN: Neuron simulation (LIF, Izhikevich)
     * - CNN: Audio/visual/speech processing
     * - LNN: ODE solvers for Liquid Neural Networks
     * - Quantum: GPU-accelerated quantum algorithm simulation
     *
     * FALLBACK: If no GPU available or disabled, uses CPU execution.
     *
     * GPU-FIRST DEFAULT (Phase 1 GPU Integration):
     * As of v2.6.3, GPU is the DEFAULT backend. The system will automatically:
     * 1. Try CUDA (NVIDIA GPUs)
     * 2. Try ROCm (AMD GPUs)
     * 3. Try OpenCL (cross-platform)
     * 4. Fall back to CPU only if no GPU is available
     *
     * To force CPU-only execution, set force_cpu_only = true.
     * The legacy disable_gpu field is kept for backward compatibility.
     */
    bool disable_gpu;                     /**< Disable GPU even if available (default: false) [DEPRECATED: use force_cpu_only] */

    /**
     * Force CPU-only execution (Phase 1 GPU Integration)
     *
     * WHAT: Force CPU execution even when GPU is available
     * WHY:  Semantic inversion - GPU is now the default, this opts out
     * HOW:  Set to true to skip GPU initialization entirely
     *
     * USAGE:
     * ```c
     * // Force CPU-only (skip GPU detection and init)
     * config.force_cpu_only = true;
     *
     * // Use GPU if available (default behavior)
     * config.force_cpu_only = false;  // or just don't set it
     * ```
     *
     * NOTE: This is the preferred way to disable GPU. The older
     * disable_gpu field is kept for backward compatibility but
     * force_cpu_only takes precedence if both are set.
     */
    bool force_cpu_only;                  /**< Force CPU-only execution, skip GPU init (default: false) */

    int32_t gpu_device_id;                /**< GPU device ID (-1 = auto-select best) (default: -1) */
    bool gpu_enable_async;                /**< Enable async GPU operations (default: true) */
    uint32_t gpu_batch_size;              /**< Preferred batch size for GPU ops (default: 256) */

    // === GPU INFERENCE OPTIMIZATION ===
    uint32_t inference_threads;           /**< Thread pool size for parallel stages (0 = auto/4, >0 = explicit) */
    bool force_serial_inference;          /**< Disable parallel cognitive stages (default: false) */

    // === INITIALIZATION MODE & PARALLELISM ===
    brain_init_mode_t init_mode;          /**< Init depth: FULL/FAST/MINIMAL (default: FULL) */
    bool parallel_init;                   /**< Enable parallel subsystem init via wave executor (default: true) */
    uint32_t init_threads;               /**< Thread pool size for parallel init (0 = auto/4, >0 = explicit) */
    uint32_t wiring_threads;             /**< Thread pool size for backbone wiring (0 = auto/4, >0 = explicit) */

    // === PHASE 4 NEUROMODULATORY NUCLEI CONFIGURATION ===
    /**
     * Phase 4 Neuromodulatory Nuclei Configuration
     *
     * WHAT: Enable and configure individual neuromodulatory nuclei
     * WHY:  Fine-grained control over neuromodulation systems
     * HOW:  Each nucleus can be enabled independently with custom parameters
     *
     * NUCLEI:
     * - LC (Locus Coeruleus): Norepinephrine (NE) - arousal, attention, stress response
     * - VTA (Ventral Tegmental Area): Dopamine (DA) - reward, motivation, learning
     * - Raphe Nuclei: Serotonin (5-HT) - mood, impulse control, patience
     * - Habenula: Aversion - disappointment, negative outcomes, avoidance
     *
     * INTEGRATION:
     * - Security System: Threat response modulation
     * - Immune System: Psychoneuroimmunology (cytokine-neuromodulator crosstalk)
     * - Logging System: Audit trails for neuromodulatory events
     * - Training System: Bidirectional learning rate modulation
     * - Bio-Async: Cross-nuclei coordination messaging
     */
    bool enable_lc;                       /**< Enable Locus Coeruleus (NE) (default: false) */
    bool enable_vta;                      /**< Enable Ventral Tegmental Area (DA) (default: false) */
    bool enable_raphe;                    /**< Enable Raphe Nuclei (5-HT) (default: false) */
    bool enable_habenula;                 /**< Enable Habenula (aversion) (default: false) */
    bool enable_neuromod_intra;           /**< Enable neuromodulatory intra-coordinator (default: false) */

    // Neuromodulatory coupling strengths (cross-nuclei interactions)
    float neuromod_lc_vta_coupling;       /**< LC-VTA coupling strength (default: 0.3) */
    float neuromod_lc_raphe_coupling;     /**< LC-Raphe coupling strength (default: 0.2) */
    float neuromod_vta_raphe_coupling;    /**< VTA-Raphe coupling strength (default: 0.25) */
    float neuromod_vta_habenula_coupling; /**< VTA-Habenula coupling strength (default: 0.4) */
    float neuromod_raphe_habenula_coupling; /**< Raphe-Habenula coupling strength (default: 0.3) */

    // Neuromodulatory system integration flags
    bool neuromod_enable_security_bridge; /**< Enable security-neuromod bridge (default: true) */
    bool neuromod_enable_immune_bridge;   /**< Enable immune-neuromod bridge (default: true) */
    bool neuromod_enable_logging_bridge;  /**< Enable logging-neuromod bridge (default: true) */
    bool neuromod_enable_training_bridge; /**< Enable training-neuromod bridge (default: true) */

    // === WORLD MODEL CONFIGURATION ===
    /**
     * World Model Configuration
     *
     * WHAT: Enable and configure generative world models for mental simulation
     * WHY:  Support counterfactual reasoning, policy evaluation, and dreaming
     * HOW:  Instantiate omni world model and/or multimodal world model
     *
     * MODELS:
     * - Omni World Model: Omnidirectional prediction (forward/backward/lateral dynamics)
     *   Uses RSSM (Recurrent State Space Model) inspired by DreamerV3
     * - Multimodal World Model: Cross-modal state prediction and sensory fusion
     *   Enables unified world representation across visual, auditory, tactile domains
     *
     * INTEGRATION:
     * - Active Inference: Policy evaluation via world model simulation
     * - Imagination Engine: Scene generation using world model dynamics
     * - Hippocampus: Memory replay and consolidation
     * - Predictive Processing: Forward model for prediction errors
     * - Sleep System: Dreaming during offline consolidation
     *
     * @see nimcp_omni_world_model.h for omni world model API
     * @see nimcp_world_model_multimodal.h for multimodal world model API
     */
    bool enable_world_model;              /**< Enable world model system (default: false) */
    bool lazy_world_model_init;           /**< Defer world model initialization (default: true) */

    // Omni World Model Configuration
    bool enable_omni_world_model;         /**< Enable omnidirectional world model (default: true if enable_world_model) */
    uint32_t omni_wm_state_dim;           /**< Omni WM state dimensionality (default: 64) */
    uint32_t omni_wm_action_dim;          /**< Omni WM action dimensionality (default: 32) */
    uint32_t omni_wm_obs_dim;             /**< Omni WM observation dimensionality (default: 64) */
    uint32_t omni_wm_latent_dim;          /**< Omni WM latent space dimension (default: 64) */
    uint32_t omni_wm_rssm_h_dim;          /**< RSSM deterministic state dim (default: 128) */
    uint32_t omni_wm_rssm_z_dim;          /**< RSSM stochastic state dim (default: 32) */
    float omni_wm_learning_rate;          /**< Omni WM learning rate (default: 0.0003) */
    bool omni_wm_enable_dreaming;         /**< Enable offline dreaming/simulation (default: true) */
    uint32_t omni_wm_dream_horizon;       /**< Dream episode length (default: 15) */

    // Multimodal World Model Configuration
    bool enable_multimodal_world_model;   /**< Enable multimodal world model (default: true if enable_world_model) */
    uint32_t mm_wm_latent_dim;            /**< Multimodal WM latent dimension (default: 256) */
    uint32_t mm_wm_max_entities;          /**< Maximum tracked entities (default: 128) */
    uint32_t mm_wm_max_prediction_steps;  /**< Maximum prediction steps (default: 50) */
    float mm_wm_learning_rate;            /**< Multimodal WM learning rate (default: 0.001) */
    bool mm_wm_enable_bio_async;          /**< Enable bio-async integration (default: true) */

    // World Model Integration Flags
    bool world_model_connect_active_inference;    /**< Connect to active inference system (default: true) */
    bool world_model_connect_imagination;         /**< Connect to imagination engine (default: true) */
    bool world_model_connect_hippocampus;         /**< Connect to hippocampus for replay (default: true) */
    bool world_model_connect_predictive;          /**< Connect to predictive processing (default: true) */

    // World Model Bridge Configuration
    // These bridges enable bidirectional information flow between the omni world
    // model and brain subsystems. Default is true if enable_world_model is true.
    bool enable_wm_security_immune_bridge;        /**< Security + Immune cytokine modulation bridge */
    bool enable_wm_logging_bridge;                /**< Audit logging integration bridge */
    bool enable_wm_cognitive_bridge;              /**< Full cognitive layer bridge */
    bool enable_wm_parietal_bridge;               /**< Spatial/physics reasoning bridge */
    bool enable_wm_hypothalamus_bridge;           /**< Homeostatic control bridge */
    bool enable_wm_thalamic_bridge;               /**< Attention gating via nuclei bridge */
    bool enable_wm_substrate_bridge;              /**< Metabolic constraints bridge */
    bool enable_wm_memory_bridge;                 /**< Hippocampus + engrams + consolidation bridge */
    bool enable_wm_kg_bridge;                     /**< Knowledge Graph wiring integration bridge */
    bool enable_wm_tom_bridge;                    /**< Theory of Mind social world modeling bridge */
    bool enable_wm_plasticity_bridge;             /**< SNN/STDP/Plasticity direct integration bridge */
    bool enable_wm_thousand_brains_bridge;        /**< Hawkins Thousand Brains cortical column integration bridge */
    bool enable_thousand_brains_integration;      /**< Enable full Thousand Brains integration hub (ref frames, voting, sequences + all wiring) */

    // === FUZZY LOGIC INTEGRATION ===
    bool enable_fuzzy_logic;                      /**< Enable fuzzy logic utility module (default: true) */

    // === CREATIVE SYSTEM INTEGRATION (Artistic Appreciation & Generation) ===
    /**
     * Creative System Configuration
     *
     * WHAT: Artistic cognition capabilities for appreciation and generation
     * WHY:  Enable AI to understand, learn from, and create art
     * HOW:  Integrates aesthetic evaluation, style learning, content generation
     *
     * CAPABILITIES:
     * - Aesthetic Appreciation: Evaluate art quality using Berlyne aesthetics
     * - Style Learning: Learn and represent artistic styles via embeddings
     * - Text Generation: Poetry, prose, screenplay, lyrics
     * - Music Generation: Composition, arrangement (MIDI/audio export)
     * - Visual Generation: Images via diffusion models and GANs
     * - Video Generation: Video synthesis and cinema production
     * - Multimodal Direction: Full-length film/creative project coordination
     * - Ethics Validation: Copyright, safety, bias detection
     *
     * STYLE ARCHETYPES:
     * - Literary: Hemingway, Tolstoy, Joyce, Poe, Austen, Shakespeare, etc.
     * - Musical: Bach, Beethoven, Debussy, John Williams, Miles Davis, etc.
     * - Visual: Van Gogh, Monet, Picasso, Dali, Warhol, Rembrandt, etc.
     * - Cinematic: Kubrick, Spielberg, Tarantino, Nolan, Tarkovsky, etc.
     *
     * EXTERNAL MODELS:
     * - Local ONNX: Stable Diffusion XL, StyleGAN2/3, MusicGen
     * - Cloud APIs: Stability AI, OpenAI DALL-E, Replicate (fallback)
     *
     * DEPENDENCIES:
     * - Emotion System: Aesthetic emotional responses
     * - Memory System: Artistic experience storage
     * - Ethics Engine: Content safety validation
     * - GPU Context: Accelerated generation
     *
     * @see cognitive/creative/nimcp_creative.h for main API
     * @see cognitive/creative/nimcp_creative_orchestrator.h for orchestrator
     */
    bool enable_creative_system;                  /**< Enable creative/artistic system (default: false) */
    bool lazy_creative_init;                      /**< Defer creative system initialization (default: true) */

    // Creative System Component Configuration
    bool enable_creative_text;                    /**< Enable text generation (poetry, prose, screenplay) */
    bool enable_creative_music;                   /**< Enable music generation (composition, MIDI/audio) */
    bool enable_creative_visual;                  /**< Enable visual generation (diffusion, GAN) */
    bool enable_creative_video;                   /**< Enable video generation and cinema */
    bool enable_creative_appreciation;            /**< Enable aesthetic appreciation subsystem */
    bool enable_creative_ethics;                  /**< Enable creative ethics validation (default: true) */

    // Creative Generation Backend Configuration
    bool creative_use_local_diffusion;            /**< Use local diffusion models (requires ONNX) */
    bool creative_use_local_gan;                  /**< Use local GAN models (requires ONNX) */
    bool creative_use_cloud_api;                  /**< Use cloud API fallback (requires API keys) */
    bool creative_auto_select_backend;            /**< Auto-select best available backend */

    // Creative Quality Configuration
    float creative_quality_threshold;             /**< Minimum quality score for generation (0-1, default: 0.7) */
    uint32_t creative_max_regeneration;           /**< Max regeneration attempts on quality fail (default: 3) */
    float creative_copyright_threshold;           /**< Max similarity to known works (0-1, default: 0.8) */

    // Direct neuron count override
    uint32_t neuron_count;                        /**< If >0, override size-preset neuron count */

    // Cognitive stage parameters (used in brain_decide())
    float reasoning_blend_weight;                 /**< Reasoning vs network blend [0-1], 0=all network (default: 0.3) */
    float dialogue_confidence_min;                /**< Min confidence to trigger inner dialogue (default: 0.2) */
    float dialogue_confidence_max;                /**< Max confidence to trigger inner dialogue (default: 0.85) */
    float imagination_confidence_min;             /**< Min confidence to trigger imagination (default: 0.3) */
    float rcog_confidence_max;                    /**< Max confidence to trigger recursive cognition (default: 0.5) */

    // ========================================================================
    // EDGE/ROBOT INTEGRATION
    // ========================================================================
    // Enable sensor hub, safety watchdog, and ROS 2 bridge for embodied
    // deployment on robots, drones, and edge devices.
    // All default to false — these are opt-in for edge deployments.
    bool enable_sensor_hub;                       /**< Enable sensor hub for heterogeneous sensor fusion (default: false) */
    bool enable_safety_watchdog;                  /**< Enable safety watchdog for actuator output validation (default: false) */
    bool enable_ros2_bridge;                      /**< Enable ROS 2 bridge for robot platform integration (default: false) */
    bool enable_mavlink_bridge;                   /**< Enable MAVLink bridge for PX4/ArduPilot FC (default: false) */
    bool enable_dji_bridge;                       /**< Enable DJI OSDK bridge for DJI Matrice FC (default: false) */
    bool enable_msp_bridge;                       /**< Enable MSP bridge for Betaflight/iNav FC (default: false) */
    bool enable_parrot_bridge;                    /**< Enable Parrot Olympe bridge for ANAFI drone (default: false) */
    bool enable_native_language;                  /**< Enable brain-native language production (default: false) */
    bool enable_brain_tokenizer;                   /**< Enable brain-native tokenizer (default: false) */

    /* === APPENDED FIELDS (checkpoint-safe: new fields at END only) ===
     * These fields were NOT in the original checkpoint format. The loader
     * memsets to 0 first, then reads as many bytes as available. Fields
     * added here default to 0/false when loading old checkpoints. */
    bool enable_intuitive_physics;                /**< Enable physics simulation engine */
    bool enable_physics_prior;                    /**< Enable physics prior on WM predictions */
    float intuitive_physics_dt;                   /**< Physics timestep (default: 0.01) */
    uint32_t intuitive_physics_max_objects;       /**< Max tracked objects (default: 128) */
    float physics_prior_weight;                   /**< Initial physics blend weight (default: 0.3) */

    /* Appended Apr 11 2026 — must stay at the END of the struct for
     * checkpoint forward compat. See nimcp_brain_persistence.c — the
     * loader memsets to 0 then reads as many bytes as the on-disk
     * checkpoint has, so new fields added here default to 0/false when
     * loading older checkpoints. The loader unconditionally re-applies
     * init_config defaults for these trailing fields post-read to avoid
     * stale zeros. */
    bool train_ann;                /**< Enable adaptive/ANN training (default: true) */

    /* SNN-only recovery mode convenience preset. When true, learn_vector
     * behaves as if train_ann=train_cnn=train_lnn=false while leaving
     * train_snn alone. Toggleable at runtime via set_snn_only_recovery. */
    bool snn_only_recovery_mode;

    /* Ensemble warmup scale [0.0, 1.0]. Probabilistic gate on non-SNN
     * training updates. Used after snn-only recovery to gradually bring
     * the ANN/CNN/LNN back into training without a sudden co-adaptation
     * jolt. On each brain_learn_vector call:
     *   - If >= 1.0 (default), non-SNN networks train every step.
     *   - If 0.0, non-SNN networks are fully frozen (equivalent to
     *     snn_only_recovery_mode without the explicit flag).
     *   - If intermediate (e.g. 0.3), non-SNN networks train with
     *     probability 0.3 per step, Monte-Carlo style — no LR math,
     *     no optimizer state interactions, each network independently
     *     rolled so warmup doesn't sync across them.
     * The SNN is never affected — it always trains when train_snn=true.
     * Toggle via set_ensemble_warmup_scale(brain, 0.0..1.0). */
    float ensemble_warmup_scale;

    /* SNN-primary architecture: target neuron counts for sub-networks.
     * 0 means "use default" (NIMCP_DEFAULT_SNN_NEURONS / NIMCP_DEFAULT_LNN_NEURONS). */
    uint32_t snn_target_neurons;            /**< Target SNN neuron count (0 = default) */
    uint32_t lnn_target_neurons;            /**< Target LNN neuron count (0 = default) */
} brain_config_t;

/* SNN-primary architecture defaults.
 *
 * The 1.8M → 1.9M bump (2026-04-27) accounts for the new language /
 * sensorymotor substrate pops created by nimcp_brain_factory_init_language_pops
 * (broca 64K + wernicke 64K + arcuate 32K + sensorymotor_ring 40K = 200K).
 * Those pops sit on top of the hierarchical SNN, so total SNN at run time is
 * ~2.1M neurons. The default knob here controls the hierarchical sub-network
 * only — bumping to 1.9M gives the hierarchy slightly more capacity (extra
 * width per tier) so the new substrate doesn't crowd cortex-side activity. */
#define NIMCP_DEFAULT_SNN_NEURONS       1800000  /**< 1.8M hierarchical SNN (+200K language/sensorymotor on top) — matches TIER_DEFS, leaves headroom under 86GB cgroup */
#define NIMCP_DEFAULT_LNN_NEURONS       512      /**< LNN cap (O(n²) adjoint, sweet spot for temporal richness) */
#define NIMCP_DEFAULT_ANN_NEURONS       150000   /**< ANN teacher (SNN is primary) */

/**
 * @brief Create brain with preset size and task
 *
 * @param task_name Human-readable name (e.g., "ethics", "coordination")
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
brain_t brain_create(const char* task_name, brain_size_t size, brain_task_t task,
                     uint32_t num_inputs, uint32_t num_outputs);

/**
 * @brief Create brain with custom configuration
 *
 * @param config Custom configuration
 * @return Brain handle or NULL on error
 */
brain_t brain_create_custom(const brain_config_t* config);

/**
 * @brief Create minimal brain for fast initialization (test/embedded use)
 *
 * WHAT: Creates brain with minimal_mode=true, skipping all optional subsystems
 * WHY:  5-10x faster initialization for tests and resource-constrained systems
 * HOW:  Sets minimal_mode before config initialization, skips cognitive systems
 *
 * ENABLED (core functionality):
 * - Neural network with STDP/Hebbian learning
 * - Memory pools for hot-path allocations
 * - Basic event bus
 *
 * DISABLED (optional subsystems):
 * - Working memory, Theory of Mind, Mirror neurons
 * - Global Workspace, Emotional systems
 * - Glial integration, Myelin sheath
 * - Visual/Audio/Speech cortices
 * - Ethics, Empathy, Introspection
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
brain_t brain_create_minimal(const char* task_name, brain_size_t size, brain_task_t task,
                             uint32_t num_inputs, uint32_t num_outputs);

/**
 * @brief Create brain with lazy initialization for heavy subsystems
 *
 * WHY: 2-5x faster initialization by deferring heavy subsystems until first use.
 * Unlike minimal_mode, all subsystems ARE enabled - they're just initialized lazily.
 * This is ideal for production use where startup time matters but full functionality
 * is eventually needed.
 *
 * Heavy subsystems deferred (initialized on first access):
 * - Glial integration (astrocytes, oligodendrocytes)
 * - Axon network (myelination modeling)
 * - Dendrite network (dendritic computation)
 * - Visual/Audio/Speech cortices
 * - Working memory
 * - Theory of Mind (agent modeling)
 * - Global Workspace (conscious access)
 * - Ethics Engine (value alignment)
 * - Mirror Neurons (imitation learning)
 * - Memory Consolidation (engram formation)
 * - Meta-Learning (learning-to-learn)
 * - Executive Functions (Portia integration)
 *
 * Core subsystems always initialized immediately:
 * - Neural network core
 * - Event bus
 * - Memory pools
 * - Pink noise neuromodulation
 * - Attention, Brain regions, Curiosity, Salience
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
brain_t brain_create_lazy(const char* task_name, brain_size_t size, brain_task_t task,
                          uint32_t num_inputs, uint32_t num_outputs);

/**
 * @brief Create brain with fast initialization — core + training only
 *
 * WHY: 3-5x faster than full init. Only initializes subsystems required for
 * training and inference. All other subsystems are deferred to first use.
 * Recommended for large brains (>100K neurons) in production.
 *
 * INITIALIZED IMMEDIATELY:
 * - Neural network + synapse pools
 * - GPU context + inference + weight cache
 * - Security + immune system + signal handler
 * - Training pipeline + plasticity coordinator
 * - STDP/eligibility bridges
 *
 * DEFERRED (initialized on first access):
 * - All cognitive systems (working memory, ToM, ethics, etc.)
 * - All perception systems (visual, audio, speech)
 * - Structural subsystems (glial, axon, dendrite)
 * - Memory systems (consolidation, world model)
 * - Biological systems (medulla, hypothalamus, etc.)
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
brain_t brain_create_fast(const char* task_name, brain_size_t size, brain_task_t task,
                          uint32_t num_inputs, uint32_t num_outputs);

//=============================================================================
// Configuration Profile Functions
//=============================================================================

/**
 * @brief Create a brain configuration from a predefined profile
 *
 * WHAT: Initialize a brain_config_t with profile-appropriate defaults
 * WHY:  Simplify configuration - no need to set 90+ flags manually
 * HOW:  Apply profile-specific settings to all configuration fields
 *
 * @param profile The configuration profile to use
 * @return Initialized brain_config_t with profile defaults
 *
 * EXAMPLE:
 * ```c
 * brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
 * config.size = BRAIN_SIZE_MEDIUM;
 * config.num_inputs = 128;
 * config.num_outputs = 10;
 * strncpy(config.task_name, "my_task", sizeof(config.task_name) - 1);
 * brain_t brain = brain_create_custom(&config);
 * ```
 *
 * NOTE: You must still set size, num_inputs, num_outputs, and task_name
 */
brain_config_t brain_config_from_profile(brain_config_profile_t profile);

/**
 * @brief Get the name of a configuration profile
 *
 * @param profile The profile to get the name for
 * @return Human-readable profile name (e.g., "STANDARD", "COGNITIVE")
 */
const char* brain_config_profile_name(brain_config_profile_t profile);

/**
 * @brief Get a description of what a profile enables
 *
 * @param profile The profile to describe
 * @return Detailed description string
 */
const char* brain_config_profile_description(brain_config_profile_t profile);

//=============================================================================
// Configuration Validation Functions
//=============================================================================

/**
 * @brief Validate a brain configuration
 *
 * WHAT: Check configuration for errors and inconsistencies
 * WHY:  Catch configuration errors before brain creation
 * HOW:  Validate all fields, check dependencies, verify compatibility
 *
 * CHECKS PERFORMED:
 * - NULL pointer check
 * - Size and task validity
 * - Input/output dimensions > 0
 * - Learning rate in range [0.0, 1.0]
 * - Sparsity target in range [0.0, 1.0]
 * - Feature dependencies (e.g., visual_cortex needs multimodal_integration)
 * - Incompatible combinations (e.g., minimal_mode + heavy features)
 *
 * @param config The configuration to validate
 * @param error  Output: error details if validation fails (can be NULL)
 * @return true if configuration is valid, false otherwise
 */
bool brain_config_validate(const brain_config_t* config, brain_config_error_t* error);

/**
 * @brief Validate and auto-fix configuration issues where possible
 *
 * WHAT: Validate configuration and attempt to fix minor issues
 * WHY:  Provide a more forgiving configuration experience
 * HOW:  Enable missing dependencies, clamp values to valid ranges
 *
 * @param config The configuration to validate and fix (modified in place)
 * @param error  Output: error details if unfixable error found (can be NULL)
 * @return true if configuration is valid (possibly after fixes), false if unfixable
 */
bool brain_config_validate_and_fix(brain_config_t* config, brain_config_error_t* error);

//=============================================================================
// Configuration Builder Pattern Functions
//=============================================================================

/**
 * @brief Create a configuration builder starting from a profile
 *
 * @param base Base profile to start from
 * @return Initialized brain_config_t ready for builder modifications
 */
brain_config_t brain_config_builder_create(brain_config_profile_t base);

/**
 * @brief Enable a feature by name
 *
 * SUPPORTED FEATURES:
 * - "working_memory", "theory_of_mind", "ethics", "empathy", "mirror_neurons"
 * - "global_workspace", "introspection", "salience", "consolidation", "curiosity"
 * - "visual_cortex", "audio_cortex", "speech_cortex", "multimodal"
 * - "glial", "oscillations", "myelin", "meta_learning", "predictive"
 * - "executive", "emotional_tagging", "sleep_wake", "mental_health"
 * - "logic", "epistemic_filter", "brain_regions", "cortical_columns"
 * - "attention", "parietal", "dragonfly", "fault_tolerance"
 * - "bio_security", "bbb_protection", "brain_immune", "security_monitoring"
 * - "training_integration", "distributed", "collective_cognition"
 *
 * @param config The configuration to modify
 * @param feature Name of the feature to enable
 * @return Modified configuration (for method chaining)
 */
brain_config_t brain_config_builder_enable(brain_config_t config, const char* feature);

/**
 * @brief Disable a feature by name
 *
 * @param config The configuration to modify
 * @param feature Name of the feature to disable
 * @return Modified configuration (for method chaining)
 */
brain_config_t brain_config_builder_disable(brain_config_t config, const char* feature);

/**
 * @brief Check if a feature is enabled by name
 *
 * @param config The configuration to query
 * @param feature Name of the feature to check
 * @return true if the feature is enabled, false otherwise
 */
bool brain_config_is_feature_enabled(const brain_config_t* config, const char* feature);

/**
 * @brief Get list of all enabled features
 *
 * @param config The configuration to query
 * @param buffer Output buffer for feature names (comma-separated)
 * @param buffer_size Size of the output buffer
 * @return Number of enabled features
 */
uint32_t brain_config_get_enabled_features(const brain_config_t* config,
                                            char* buffer, size_t buffer_size);

/**
 * @brief Print configuration summary to stdout
 *
 * @param config The configuration to print
 */
void brain_config_print_summary(const brain_config_t* config);

/**
 * @brief Destroy brain
 *
 * @param brain Brain to destroy
 */
void brain_destroy(brain_t brain);

//=============================================================================
// Phase 2.8: Dynamic Brain Resizing
//=============================================================================

/**
 * @brief Resize brain to new neuron count
 *
 * WHAT: Expand brain capacity while preserving all learned knowledge
 * WHY:  Enable continuous learning without hitting capacity limits
 * HOW:  Create new network, transfer neurons/weights, swap atomically
 *
 * GUARANTEES:
 * - Zero knowledge loss: All weights, biases, states preserved exactly
 * - Atomic operation: Network swap is instantaneous
 * - Memory safe: Old network destroyed only after successful transfer
 *
 * @param brain Brain to resize
 * @param new_neuron_count New total neuron count (must be > current)
 * @return true on success, false on error (brain unchanged)
 */
bool brain_resize(brain_t brain, uint32_t new_neuron_count);

/**
 * @brief Internal helper for brain_resize - update subsystems after network swap
 *
 * WHAT: Updates glial integration and other subsystems to reference new network
 * WHY:  brain_resize.c can't access full brain struct, so needs helper in main module
 * HOW:  Destroys/recreates glial integration with new network reference
 *
 * @param brain Brain handle
 * @param new_base_network New neural network after resize
 * @param new_neuron_count New neuron count after resize
 * @return true on success
 *
 * NOTE: This is an internal function only called by brain_resize()
 */
bool brain_resize_update_subsystems_internal(brain_t brain, neural_network_t new_base_network, uint32_t new_neuron_count);

/**
 * @brief Auto-resize brain based on utilization metrics
 *
 * WHAT: Automatically grow brain when capacity is saturated
 * WHY:  Enable continuous learning without manual intervention
 * HOW:  Evaluate metrics, decide if growth needed, resize if so
 *
 * GROWTH TRIGGERS:
 * - High utilization: >90% neurons active for >1000 steps
 * - Weight saturation: >80% weights near ±1.0
 *
 * @param brain Brain to potentially resize
 * @return true if resize occurred, false if no resize needed
 */
bool brain_auto_resize(brain_t brain);

/**
 * @brief Get brain current neuron count
 *
 * WHAT: Return current brain capacity in neurons
 * WHY:  Allow monitoring of brain size for metrics/logging
 *
 * @param brain Brain to query
 * @return Neuron count, or 0 on error
 */
uint32_t brain_get_neuron_count(brain_t brain);

/**
 * @brief Get brain utilization metrics
 *
 * WHAT: Return current capacity utilization statistics
 * WHY:  Enable monitoring and debugging of auto-resize logic
 *
 * @param brain Brain to analyze
 * @param utilization Output: neuron utilization ratio [0.0, 1.0]
 * @param saturation Output: weight saturation ratio [0.0, 1.0]
 * @return true on success, false on error
 */
bool brain_get_utilization_metrics(brain_t brain, float* utilization, float* saturation);

/**
 * @brief Get working memory from brain (Phase 10.2 accessor)
 *
 * WHAT: Retrieve pointer to brain's working memory subsystem
 * WHY:  Allow API wrapper functions to access working memory
 * HOW:  Return brain->working_memory field
 *
 * @param brain Brain instance
 * @return Working memory pointer or NULL if not enabled
 */
working_memory_t* brain_get_working_memory(brain_t brain);

/**
 * @brief Get executive controller from brain
 *
 * WHAT: Retrieve pointer to brain's executive control subsystem
 * WHY:  Allow cognitive modules to access executive function stats
 * HOW:  Return brain->executive field
 *
 * @param brain Brain instance
 * @return Executive controller pointer or NULL if not enabled
 */
executive_controller_t* brain_get_executive(brain_t brain);

/**
 * @brief Get global workspace from brain
 *
 * WHAT: Retrieve pointer to brain's global workspace subsystem
 * WHY:  Allow cognitive modules to access workspace for competition and broadcasting
 * HOW:  Return brain->global_workspace field
 *
 * @param brain Brain instance
 * @return Global workspace pointer or NULL if not enabled
 */
global_workspace_t* brain_get_global_workspace(brain_t brain);

/**
 * @brief Get parietal lobe from brain (Phase 7.2 accessor)
 *
 * WHAT: Retrieve pointer to brain's parietal lobe subsystem
 * WHY:  Allow access to mathematical/scientific reasoning capabilities
 * HOW:  Return brain->parietal field
 *
 * CAPABILITIES:
 * - Number Sense: Magnitude estimation, subitizing, comparison
 * - Spatial Reasoning: Mental rotation, coordinate transforms
 * - Mathematical Intuition: Pattern detection, analogical reasoning
 * - Scientific Reasoning: Hypothesis testing, dimensional analysis
 * - Equation Manipulation: Symbolic math, differentiation
 *
 * @param brain Brain instance
 * @return Parietal lobe pointer or NULL if not enabled
 */
parietal_lobe_t* brain_get_parietal(brain_t brain);

/**
 * @brief Update parietal from immune system state
 *
 * Synchronizes inflammation levels with parietal precision.
 * Higher inflammation increases Weber fraction (reduces numerical accuracy).
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_update_parietal_from_immune(brain_t brain);

/**
 * @brief Update parietal from sleep system state
 *
 * Synchronizes fatigue levels with parietal accuracy.
 * Sleep deprivation reduces spatial and numerical reasoning performance.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_update_parietal_from_sleep(brain_t brain);

/**
 * @brief Step parietal lobe forward in time
 *
 * Processes pending parietal requests and updates neural state.
 * Call this during brain stepping for continuous parietal processing.
 *
 * @param brain Brain instance
 * @param delta_t Time step in microseconds
 * @return 0 on success, -1 on error
 */
int brain_step_parietal(brain_t brain, uint64_t delta_t);

//=============================================================================
// Dragonfly Integration (Bio-inspired Target Tracking)
//=============================================================================

/**
 * @brief Get dragonfly system from brain
 *
 * WHAT: Retrieve pointer to brain's dragonfly target tracking subsystem
 * WHY:  Allow access to bio-inspired target tracking and interception
 * HOW:  Return brain->dragonfly field
 *
 * CAPABILITIES:
 * - TSDN: Population vector encoding of target direction (16 neurons)
 * - CSTMD1: Winner-take-all selective attention for target lock
 * - Prediction: IMM filter trajectory prediction with evasion detection
 * - Interception: Proportional navigation guidance for pursuit
 *
 * BIOLOGICAL BASIS:
 * Dragonflies achieve 95% hunting success through:
 * - Internal models predicting prey trajectory
 * - Parallel processing in small target motion detector neurons
 * - Predictive gain modulation along expected target path
 *
 * @param brain Brain instance
 * @return Dragonfly system pointer or NULL if not enabled
 */
dragonfly_system_t* brain_get_dragonfly(brain_t brain);

/**
 * @brief Update dragonfly with visual detection
 *
 * Process a new visual detection through the dragonfly tracking system.
 * This feeds the TSDN and CSTMD1 networks with target information.
 *
 * @param brain Brain instance
 * @param position Target position [x, y, z]
 * @param size Target angular size in radians
 * @param contrast Target contrast against background [0-1]
 * @return 0 on success, -1 on error
 */
int brain_dragonfly_detect(brain_t brain, const float position[3],
                           float size, float contrast);

/**
 * @brief Get current dragonfly motor command
 *
 * Retrieve the current pursuit/interception motor command from dragonfly.
 * Commands include heading, pitch, velocity, and urgency.
 *
 * @param brain Brain instance
 * @param heading_rad Output: desired heading angle
 * @param pitch_rad Output: desired pitch angle
 * @param velocity Output: desired velocity [3]
 * @param urgency Output: command urgency [0-1]
 * @return 0 on success, -1 on error or no active pursuit
 */
int brain_dragonfly_get_command(brain_t brain, float* heading_rad,
                                float* pitch_rad, float velocity[3],
                                float* urgency);

/**
 * @brief Step dragonfly system forward in time
 *
 * Processes tracking, prediction, and interception planning.
 * Call this during brain stepping for continuous dragonfly processing.
 *
 * @param brain Brain instance
 * @param delta_t Time step in microseconds
 * @return 0 on success, -1 on error
 */
int brain_step_dragonfly(brain_t brain, uint64_t delta_t);

/**
 * @brief Get dragonfly operating mode
 *
 * @param brain Brain instance
 * @return Current mode (IDLE, SCANNING, TRACKING, PURSUING, INTERCEPTING)
 */
int brain_dragonfly_get_mode(brain_t brain);

/**
 * @brief Abort current dragonfly hunt
 *
 * Immediately terminates any active pursuit and returns to scanning mode.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_dragonfly_abort(brain_t brain);

//=============================================================================
// Dragonfly-Medulla Integration
//=============================================================================

/**
 * @brief Get dragonfly-medulla integration bridge
 *
 * Returns the bridge that connects dragonfly target tracking to medulla
 * oblongata states for arousal/protection-based modulation.
 *
 * @param brain Brain instance
 * @return Bridge handle or NULL if not connected
 */
dragonfly_medulla_bridge_t brain_get_dragonfly_medulla_bridge(brain_t brain);

/**
 * @brief Get current dragonfly modulation state from medulla
 *
 * Returns the current modulation factors applied to dragonfly based on
 * arousal level, protection level, and circadian phase.
 *
 * MODULATION EFFECTS:
 * - Arousal: Scales nav gain, urgency, reaction time, accuracy
 * - Protection: Can block/abort hunting, limit pursuit duration
 * - Circadian: Performance modulation (peak at morning, low at night)
 *
 * @param brain Brain instance
 * @param modulation Output modulation state
 * @return 0 on success, -1 on error
 */
int brain_dragonfly_get_modulation(brain_t brain,
                                   dragonfly_medulla_modulation_t* modulation);

/**
 * @brief Check if dragonfly hunting is currently allowed
 *
 * Quick check based on current medulla states. Returns false if:
 * - Protection level is CRITICAL or SHUTDOWN
 * - Arousal level is too low (COMA, DEEP_SLEEP)
 * - Circadian phase indicates nighttime inactivity
 *
 * @param brain Brain instance
 * @return true if hunting is allowed based on medulla state
 */
bool brain_dragonfly_hunting_allowed(brain_t brain);

//=============================================================================
// Knowledge Graph Reader (Self-Awareness)
//=============================================================================

/**
 * @brief Get knowledge graph reader from brain
 *
 * WHAT: Retrieve pointer to brain's KG reader for self-awareness queries
 * WHY:  Enable runtime introspection of NIMCP's structural knowledge
 * HOW:  Return brain->kg_reader field
 *
 * SELF-AWARENESS CAPABILITIES:
 * - "What modules do I have?" - Component enumeration
 * - "How am I organized?" - Structural relationships
 * - "What can I do?" - Capability introspection
 *
 * The KG reader loads from .aim/memory-nimcp.jsonl which contains:
 * - Entities: Modules, integrations, conventions, architectures
 * - Relations: How components connect and interact
 * - Observations: Capabilities, file locations, test status
 *
 * @param brain Brain instance
 * @return KG reader pointer or NULL if not enabled
 */
kg_reader_t* brain_get_kg_reader(brain_t brain);

/**
 * @brief Reload knowledge graph from file
 *
 * Call this if the KG file has been updated externally to refresh
 * the brain's structural self-knowledge.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_reload_kg(brain_t brain);

/**
 * @brief Check if KG file has been modified since last load
 *
 * Useful for detecting when external updates require a reload.
 *
 * @param brain Brain instance
 * @return true if file was modified, false otherwise
 */
bool brain_is_kg_modified(brain_t brain);

/**
 * @brief Generate self-description from KG
 *
 * Creates a human-readable description of the system based on KG contents.
 * Useful for self-model verbalization and debugging.
 *
 * @param brain Brain instance
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written, or -1 on error
 */
int brain_generate_self_description(brain_t brain, char* buffer, size_t buffer_size);

/**
 * WHAT: Retrieve pointer to brain's sleep/wake subsystem (Phase 10.4)
 * WHY:  Allow external control of sleep cycles and pressure monitoring
 * HOW:  Return brain->sleep_system field
 *
 * @param brain Brain instance
 * @return Sleep system pointer or NULL if invalid brain
 */
sleep_system_t brain_get_sleep_system(brain_t brain);

/**
 * WHAT: Retrieve pointer to brain's medulla oblongata subsystem
 * WHY:  Allow external access to autonomic regulation (arousal, protection, circadian)
 * HOW:  Return brain->medulla field
 *
 * NOTE: Primarily used for testing. Use specific accessor functions for
 * querying arousal level, protection level, circadian phase etc.
 *
 * @param brain Brain instance
 * @return Medulla pointer or NULL if not enabled/invalid brain
 */
medulla_t brain_get_medulla(brain_t brain);

/**
 * WHAT: Retrieve pointer to brain's Theory of Mind subsystem (Phase 10.6)
 * WHY:  Allow external access to social cognition and empathy functions
 * HOW:  Return brain->theory_of_mind field
 *
 * @param brain Brain instance
 * @return Theory of Mind pointer or NULL if not enabled/invalid brain
 */
theory_of_mind_t brain_get_theory_of_mind(brain_t brain);

/**
 * @brief Get explanation generator from brain (Phase 10.7)
 *
 * @param brain Brain instance
 * @return Explanation generator pointer or NULL if not enabled/invalid brain
 */
explanation_generator_t brain_get_explanation_generator(brain_t brain);

//=============================================================================
// Phase 2: Copy-on-Write Brain Cloning
//=============================================================================

/**
 * @brief Clone brain using copy-on-write (COW) optimization
 *
 * WHAT: Creates lightweight clone sharing memory with original
 * WHY:  Enable efficient replication with 86% memory savings
 * HOW:  Shares network structure, copies on first write
 *
 * @param original Brain to clone
 * @return Cloned brain or NULL on error
 */
brain_t brain_clone_cow(brain_t original);

//=============================================================================
// Phase 2.8: Distributed Copy-on-Write Brain Cloning
//=============================================================================

// Forward declaration for distributed COW types
typedef struct distributed_cow_config distributed_cow_config_t;
typedef struct distributed_cow_stats distributed_cow_stats_t;

/**
 * @brief Create distributed COW clone on remote node
 *
 * WHAT: Creates a COW clone that fetches network data from remote master
 * WHY:  Enable efficient distributed deployment without full network copy
 * HOW:  Establishes P2P connection, initializes cache, registers with master
 *
 * @param original Brain to clone (must be on master node)
 * @param remote_host Master node hostname/IP
 * @param remote_port Master node port
 * @param config Distributed COW configuration (NULL for defaults)
 * @return Distributed COW clone or NULL on error
 *
 * MEMORY: ~1-10MB overhead (cache + metadata)
 * LATENCY: ~10-100ms (connection + handshake)
 * BANDWIDTH: Lazy loading - only fetches neurons needed for inference
 *
 * Example:
 * ```c
 * // On master node (192.168.1.100:5000)
 * brain_t master = brain_create("model", BRAIN_SIZE_MEDIUM,
 *                               BRAIN_TASK_CLASSIFICATION, 256, 10);
 * brain_enable_distributed_cow_master(master, p2p_node);
 *
 * // On remote node (192.168.1.101)
 * brain_t clone = brain_clone_cow_distributed(master, "192.168.1.100", 5000, NULL);
 * // Clone shares network with master, fetches on demand
 * brain_decision_t* decision = brain_decide(clone, input, 256);
 * ```
 */
NIMCP_EXPORT brain_t brain_clone_cow_distributed(
    brain_t original,
    const char* remote_host,
    uint16_t remote_port,
    const distributed_cow_config_t* config
);

/**
 * @brief Enable distributed COW serving on master node
 *
 * WHAT: Configures brain to serve network segments to remote clones
 * WHY:  Allow brain to act as master for distributed COW
 * HOW:  Registers network segment handlers, starts P2P listener
 *
 * @param brain Brain to enable as master
 * @param p2p_node P2P node for serving (must be started)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool brain_enable_distributed_cow_master(
    brain_t brain,
    p2p_node_t p2p_node
);

/**
 * @brief Get distributed COW statistics
 *
 * @param brain Brain handle
 * @param stats Output statistics structure
 * @return true on success, false if not distributed COW
 */
NIMCP_EXPORT bool brain_get_distributed_cow_stats(
    brain_t brain,
    distributed_cow_stats_t* stats
);

/**
 * @brief Check if brain is distributed COW clone
 *
 * @param brain Brain handle
 * @return true if distributed COW clone, false otherwise
 */
NIMCP_EXPORT bool brain_is_distributed_cow(brain_t brain);

//=============================================================================
// Phase 3: Distributed Brain API
//=============================================================================

/**
 * @brief Create distributed brain with P2P coordination
 *
 * WHAT: Creates a brain that can coordinate with peer brains over network
 * WHY:  Enable multi-brain collaborative learning and shared chemical signals
 * HOW:  Integrates distributed cognition coordinator for neuromod/glial sync
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param p2p_node P2P network node for coordination
 * @return Distributed brain handle or NULL on error
 *
 * THREAD SAFETY: Thread-safe creation
 * PERFORMANCE: O(n) where n = num_neurons + network initialization
 */
NIMCP_EXPORT brain_t brain_create_distributed(
    const char* task_name,
    brain_size_t size,
    brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    p2p_node_t p2p_node
);

/**
 * @brief Enable distributed coordination on existing brain
 *
 * WHAT: Retrofits an existing brain with distributed cognition
 * WHY:  Allow conversion of standalone brain to distributed mode
 * HOW:  Creates distrib_cognition coordinator and starts sync threads
 *
 * @param brain Brain handle
 * @param p2p_node P2P network node
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe if brain not actively processing
 */
NIMCP_EXPORT bool brain_enable_distributed(brain_t brain, p2p_node_t p2p_node);

/**
 * @brief Synchronize neuromodulators with peer brains
 *
 * WHAT: Manually trigger neuromodulator broadcast to network
 * WHY:  Allow explicit control of sync timing for performance tuning
 * HOW:  Calls distrib_cognition_broadcast_neuromod for all neuromod types
 *
 * @param brain Distributed brain handle
 * @return true on success, false if not distributed or error
 *
 * THREAD SAFETY: Thread-safe
 * PERFORMANCE: O(P × N) where P=peers, N=neuromod types
 */
NIMCP_EXPORT bool brain_sync_neuromodulators(brain_t brain);

/**
 * @brief Get distributed cognition statistics
 *
 * WHAT: Query network sync stats (broadcasts, latency, peer count)
 * WHY:  Monitor distributed brain performance and health
 * HOW:  Forwards query to underlying distrib_cognition coordinator
 *
 * @param brain Distributed brain handle
 * @param stats Output statistics structure
 * @return true on success, false if not distributed
 */
NIMCP_EXPORT bool brain_get_distributed_stats(
    brain_t brain,
    distrib_cognition_stats_t* stats
);

/**
 * @brief Check if brain is distributed
 *
 * @param brain Brain handle
 * @return true if distributed coordination enabled, false otherwise
 */
NIMCP_EXPORT bool brain_is_distributed(brain_t brain);

//=============================================================================
// Learning API
//=============================================================================

/**
 * @brief Simple feature vector for learning
 */
typedef struct {
    float* features;       /**< Feature values */
    uint32_t num_features; /**< Number of features */
    char label[64];        /**< Semantic label */
    float confidence;      /**< Training confidence (0-1) */
} brain_example_t;

/**
 * @brief Learn from single labeled example
 *
 * @param brain Brain handle
 * @param features Input features
 * @param num_features Feature count
 * @param label Target label/output
 * @param confidence Training weight (0-1)
 * @return Loss value
 */
float brain_learn_example(brain_t brain, const float* features, uint32_t num_features,
                          const char* label, float confidence);

/**
 * @brief Learn from batch of examples
 *
 * @param brain Brain handle
 * @param examples Array of examples
 * @param num_examples Example count
 * @return Average loss
 */
float brain_learn_batch(brain_t brain, const brain_example_t* examples, uint32_t num_examples);

/**
 * @brief Apply reward-based reinforcement learning to all synapses
 *
 * WHAT: Apply eligibility-trace-based learning with reward signal
 * WHY:  Enable temporal credit assignment and reinforcement learning
 * HOW:  Use eligibility traces to propagate reward to recently active synapses
 *
 * BIOLOGY: Implements three-factor learning rule (Hebbian + Reward + Dopamine)
 * - Eligibility traces mark recently active synapses ("synaptic tags")
 * - Dopamine bursts trigger consolidation ("capture")
 * - Reward signal modulates weight changes
 *
 * @param brain Brain handle
 * @param reward Reward signal (0-1 for positive, -1-0 for punishment)
 * @return Number of synapses modified
 */
uint32_t brain_apply_reward_learning(brain_t brain, float reward);

/**
 * @brief LLM teacher function signature
 *
 * @param input Input features
 * @param num_features Feature count
 * @param context User context
 * @param output_label Output buffer for decision label
 * @param max_label_len Maximum label length
 * @return Confidence in decision (0-1)
 */
typedef float (*llm_teacher_fn_t)(const float* input, uint32_t num_features, void* context,
                                  char* output_label, uint32_t max_label_len);

/**
 * @brief Learn by querying an LLM
 *
 * Allows brain to learn from any external decision maker:
 * - LLM APIs (Claude, GPT, etc.)
 * - Rule engines
 * - Human experts
 * - Other ML models
 *
 * @param brain Brain handle
 * @param input Input features
 * @param num_features Feature count
 * @param llm_fn Teacher function
 * @param llm_context Context for teacher
 * @return Loss value
 */
float brain_learn_from_llm(brain_t brain, const float* input, uint32_t num_features,
                           llm_teacher_fn_t llm_fn, void* llm_context);

//=============================================================================
// Inference API
//=============================================================================

/**
 * @brief Decision result
 *
 * NOTE: This file is DEPRECATED. Use include/core/brain/nimcp_brain.h instead.
 * This file only exists for backwards compatibility and should not be modified.
 * See include/core/brain/nimcp_brain.h for the authoritative version with CoW support.
 *
 * Phase 1.5 CoW: Decisions support copy-on-write for efficient caching.
 * When copy_decision() is called, the copy shares data with the original
 * via reference counting. Data is only deep-copied when modified.
 */
typedef struct brain_decision {
    char label[64];       /**< Decision label */
    float confidence;     /**< Confidence (0-1) */
    float* output_vector; /**< Raw output vector */
    uint32_t output_size; /**< Output vector size */

    // Interpretability (if enabled)
    uint32_t num_active_neurons; /**< Active neuron count */
    uint32_t* active_neuron_ids; /**< Active neuron IDs */
    float sparsity;              /**< Actual sparsity */
    char explanation[256];       /**< Human-readable explanation */

    uint64_t inference_time_us; /**< Inference time (microseconds) */

    // Phase 1.5: Copy-on-Write support for efficient decision caching
    uint32_t* _cow_refcount;     /**< Shared reference count (NULL = owned, else shared) */
    bool _cow_is_shallow;        /**< True if this is a shallow copy (shares pointers) */

    // Communication layer: Cognitive transcript capturing all stage outputs
    struct cognitive_transcript* transcript; /**< Full cognitive transcript (NULL if not requested) */
} brain_decision_t;

/**
 * @brief Make decision for input
 *
 * @param brain Brain handle
 * @param features Input features
 * @param num_features Feature count
 * @return Decision result (caller must free with brain_free_decision)
 */
brain_decision_t* brain_decide(brain_t brain, const float* features, uint32_t num_features);

/**
 * @brief Reset neuron and LNN hidden states for clean inference
 *
 * WHAT: Clears accumulated neuron state and LNN hidden state
 * WHY:  Without reset, inference results depend on query history —
 *       LNN hidden state accumulates across unrelated inferences
 * HOW:  Calls neural_network_reset() on the main network and
 *       resets LNN neuron states if LNN is present
 *
 * @param brain Brain handle
 * @return 0 on success, -1 on error
 */
int brain_reset_inference_state(brain_t brain);

//=============================================================================
// Input Modality Gating
//=============================================================================

/** Modality bitmask flags — controls which SNN sensory bridges process input */
#define BRAIN_MODALITY_TEXT           0x01u  /**< Text/semantic embedding (default) */
#define BRAIN_MODALITY_VISUAL         0x02u  /**< Visual frame data */
#define BRAIN_MODALITY_AUDIO          0x04u  /**< Audio spectral/MFCC data */
#define BRAIN_MODALITY_SOMATOSENSORY  0x08u  /**< Touch/proprioception data */
#define BRAIN_MODALITY_SPEECH         0x10u  /**< Phoneme/speech data */
#define BRAIN_MODALITY_ALL            0x1Fu  /**< All modalities active */

/**
 * @brief Set which sensory modalities are active for input processing
 *
 * WHAT: Control which SNN sensory bridges fire during brain_decide()
 * WHY:  Text embeddings should not be fed to visual/audio/somatosensory encoders
 * HOW:  Bitmask gating — bridges only encode when their modality bit is set
 *
 * @param brain Brain handle
 * @param modality_flags Bitmask of BRAIN_MODALITY_* flags
 */
void brain_set_active_modalities(brain_t brain, uint32_t modality_flags);

/**
 * @brief Get current active modality bitmask
 *
 * @param brain Brain handle
 * @return Current modality flags
 */
uint32_t brain_get_active_modalities(brain_t brain);

/**
 * @brief Stage raw sensory data for the next brain_decide() call
 *
 * WHAT: Submit modality-specific data that SNN bridges will consume
 * WHY:  Text embeddings should not be fed to visual/audio encoders;
 *       each modality needs its native data format
 * HOW:  Data is copied internally and consumed (freed) after next brain_decide().
 *       Automatically sets the modality bit in active_modalities.
 *
 * @param brain Brain handle
 * @param modality One of BRAIN_MODALITY_VISUAL, _AUDIO, _SPEECH, _SOMATOSENSORY
 * @param data Raw data (uint8_t* for visual, float* for others)
 * @param size Element count (pixels for visual, floats for others)
 * @param width Frame width (visual only, 0 for others)
 * @param height Frame height (visual only, 0 for others)
 * @param channels 1=gray/mono, 3=RGB, 2=stereo (0 for speech/somatosensory)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int brain_submit_sensory(brain_t brain, uint32_t modality,
                                       const void* data, uint32_t size,
                                       uint32_t width, uint32_t height,
                                       uint32_t channels);

/**
 * @brief Clear all staged sensory data without processing
 */
NIMCP_EXPORT void brain_clear_sensory(brain_t brain);

/**
 * @brief Free decision result
 *
 * @param decision Decision to free
 */
void brain_free_decision(brain_decision_t* decision);

/**
 * @brief Deep copy a brain decision (independent copy, no COW sharing)
 *
 * @param source Decision to copy
 * @return New independent decision copy, or NULL on failure
 */
brain_decision_t* copy_decision_deep(const brain_decision_t* source);

/**
 * @brief Observe action performed by another agent (Phase 10.11)
 *
 * WHAT: Record observed action in mirror neuron system for observational learning
 * WHY:  Enable learning from watching others (imitation, social cognition)
 * HOW:  Convert input features to observed action and send to mirror neurons
 *
 * This is the OBSERVATION PATHWAY for mirror neurons. When the brain observes
 * another agent performing an action, this function records it for learning.
 *
 * USE CASES:
 * - Robot watching human demonstration
 * - Agent observing another agent's behavior
 * - Learning from video/sensor data of actions
 * - Social learning and imitation
 *
 * COMPLEXITY: O(n) where n = num_features
 * THREAD-SAFE: No (requires external synchronization)
 *
 * @param brain Brain handle
 * @param features Observed action features (sensor data, visual features, etc.)
 * @param num_features Number of features
 * @param agent_id ID of agent being observed (must be > 0, as 0 = self)
 * @return true on success, false on error
 */
bool brain_observe_action(brain_t brain, const float* features, uint32_t num_features,
                          uint32_t agent_id);

/**
 * @brief Batch inference
 *
 * @param brain Brain handle
 * @param inputs Array of input vectors
 * @param num_inputs Number of inputs
 * @param features_per_input Features per input
 * @param decisions Output decisions array (allocated by caller)
 * @return true on success
 */
bool brain_decide_batch(brain_t brain, const float** inputs, uint32_t num_inputs,
                        uint32_t features_per_input, brain_decision_t* decisions);

//=============================================================================
// Async API - Non-blocking Learning and Inference
//=============================================================================

/**
 * @brief Asynchronous learning from labeled example
 *
 * WHAT: Non-blocking version of brain_learn_example that returns immediately
 * WHY:  Enable concurrent learning while continuing other operations
 * HOW:  Spawns background thread to perform learning, returns future handle
 *
 * The returned future can be used to:
 * - Check if learning completed (nimcp_future_is_ready)
 * - Wait for completion (nimcp_future_wait/nimcp_future_wait_timeout)
 * - Retrieve final loss value (nimcp_future_get)
 * - Handle errors (nimcp_future_get_error)
 *
 * THREAD SAFETY:
 * - Brain must support concurrent access or use external synchronization
 * - Future handle is thread-safe for waiting/polling from multiple threads
 * - Caller must eventually destroy future with nimcp_future_destroy()
 *
 * MEMORY MANAGEMENT:
 * - Input features are copied internally, safe to free after call returns
 * - Label string is copied internally
 * - Caller owns returned future, must call nimcp_future_destroy()
 *
 * ERROR HANDLING:
 * - Returns NULL if async system not initialized or allocation fails
 * - Learning errors propagated through future (check nimcp_future_get_error)
 *
 * COMPLEXITY: O(1) setup + O(s*n) background learning
 *
 * EXAMPLE:
 * ```c
 * // Start async learning
 * nimcp_future_t future = nimcp_brain_learn_async(
 *     brain, features, 10, "cat", 0.9f
 * );
 * if (!future) {
 *     // Handle error
 * }
 *
 * // Do other work while learning...
 * process_other_tasks();
 *
 * // Wait for completion with timeout
 * if (nimcp_future_wait_timeout(future, 1000)) {
 *     float loss;
 *     if (nimcp_future_get(future, &loss) == NIMCP_SUCCESS) {
 *         printf("Learning complete, loss: %.4f\n", loss);
 *     }
 * }
 *
 * nimcp_future_destroy(future);
 * ```
 *
 * @param brain Brain handle
 * @param features Input feature vector
 * @param num_features Number of features
 * @param label Target label string
 * @param confidence Training confidence (0-1)
 * @return Future handle or NULL on error (caller must destroy)
 */
nimcp_future_t nimcp_brain_learn_async(brain_t brain, const float* features,
                                        uint32_t num_features, const char* label,
                                        float confidence);

/**
 * @brief Asynchronous inference/decision making
 *
 * WHAT: Non-blocking version of brain_decide that returns immediately
 * WHY:  Enable concurrent inference without blocking caller
 * HOW:  Spawns background thread for forward pass, returns future handle
 *
 * The returned future can be used to:
 * - Check if inference completed (nimcp_future_is_ready)
 * - Wait for completion (nimcp_future_wait/nimcp_future_wait_timeout)
 * - Retrieve decision result (nimcp_future_get)
 * - Handle errors (nimcp_future_get_error)
 *
 * THREAD SAFETY:
 * - Brain must support concurrent access or use external synchronization
 * - Future handle is thread-safe for waiting/polling
 * - Caller must eventually destroy both future and decision
 *
 * MEMORY MANAGEMENT:
 * - Input features are copied internally, safe to free after call returns
 * - Decision result is allocated on heap, must free with brain_free_decision()
 * - Caller owns returned future, must call nimcp_future_destroy()
 *
 * ERROR HANDLING:
 * - Returns NULL if async system not initialized or allocation fails
 * - Inference errors propagated through future (check nimcp_future_get_error)
 *
 * COMPLEXITY: O(1) setup + O(s*n) background inference
 *
 * EXAMPLE:
 * ```c
 * // Start async inference
 * nimcp_future_t future = nimcp_brain_infer_async(brain, features, 10);
 * if (!future) {
 *     // Handle error
 * }
 *
 * // Do other work while inferring...
 * process_other_tasks();
 *
 * // Wait for completion with timeout
 * if (nimcp_future_wait_timeout(future, 500)) {
 *     brain_decision_t* decision;
 *     if (nimcp_future_get(future, &decision) == NIMCP_SUCCESS) {
 *         printf("Decision: %s (%.2f confidence)\n",
 *                decision->label, decision->confidence);
 *         brain_free_decision(decision);
 *     }
 * }
 *
 * nimcp_future_destroy(future);
 * ```
 *
 * @param brain Brain handle
 * @param features Input feature vector
 * @param num_features Number of features
 * @return Future handle or NULL on error (caller must destroy)
 */
nimcp_future_t nimcp_brain_infer_async(brain_t brain, const float* features,
                                        uint32_t num_features);

/**
 * @brief Simple prediction/inference function
 *
 * Lightweight wrapper around brain_process_multimodal for simple prediction tasks.
 * For full multimodal capabilities, use brain_process_multimodal directly.
 *
 * @param brain Brain handle
 * @param input Input feature vector
 * @param input_size Number of input features
 * @param output Output vector (allocated by caller)
 * @param output_size Size of output vector
 * @return true on success, false on error
 */
bool brain_predict(brain_t brain, const float* input, uint32_t input_size,
                  float* output, uint32_t output_size);

//=============================================================================
// Persistence API
//=============================================================================

/**
 * @brief Save brain to file
 *
 * @param brain Brain handle
 * @param filepath Path to save to
 * @return true on success
 */
bool brain_save(brain_t brain, const char* filepath);

/**
 * @brief Load brain from file
 *
 * @param filepath Path to load from
 * @return Brain handle or NULL on error
 */
brain_t brain_load(const char* filepath);

//=============================================================================
// Snapshot API - Compressed & Encrypted State Snapshots
//=============================================================================

/**
 * @brief Snapshot metadata
 */
typedef struct {
    char name[128];              /**< Snapshot name */
    char description[512];       /**< Human-readable description */
    uint64_t timestamp;          /**< Creation timestamp (Unix epoch) */
    uint32_t file_size;          /**< Compressed size in bytes */
    bool is_compressed;          /**< Compression enabled */
    bool is_encrypted;           /**< Encryption enabled */
} brain_snapshot_info_t;

/**
 * @brief Save brain snapshot with compression and encryption
 *
 * WHAT: Creates a named, timestamped snapshot of complete brain state
 * WHY:  Enable backups, A/B testing, version control, disaster recovery
 * HOW:  Saves to snapshot_dir with compression and encryption by default
 *
 * Snapshots are saved as: snapshot_dir/name_timestamp.snapshot
 * - Compressed with zlib (default)
 * - Encrypted with AES-256 (default)
 * - Include full brain state (network, subsystems, knowledge)
 *
 * @param brain Brain instance
 * @param name Snapshot name (e.g., "before_experiment", "v1.0")
 * @param description Optional description (can be NULL)
 * @return true on success, false on error
 */
bool brain_save_snapshot(brain_t brain, const char* name, const char* description);

/**
 * @brief Restore brain from snapshot
 *
 * WHAT: Loads brain state from named snapshot
 * WHY:  Restore previous state, rollback changes, A/B testing
 * HOW:  Decompresses and decrypts snapshot, restores all state
 *
 * @param brain Brain instance to restore into (or NULL to create new)
 * @param name Snapshot name
 * @return Brain instance (restored if provided, new if NULL), or NULL on error
 */
brain_t brain_restore_snapshot(brain_t brain, const char* name);

/**
 * @brief List available snapshots
 *
 * WHAT: Enumerate all snapshots for this brain
 * WHY:  Allow users to see available restore points
 * HOW:  Scans snapshot_dir, parses metadata
 *
 * @param brain Brain instance (for snapshot_dir config)
 * @param infos Output array of snapshot info (allocated by caller)
 * @param max_count Maximum number of snapshots to return
 * @param out_count Output: actual number of snapshots found
 * @return true on success, false on error
 */
bool brain_list_snapshots(brain_t brain, brain_snapshot_info_t* infos,
                         uint32_t max_count, uint32_t* out_count);

/**
 * @brief Delete snapshot
 *
 * @param brain Brain instance (for snapshot_dir config)
 * @param name Snapshot name to delete
 * @return true on success, false on error
 */
bool brain_delete_snapshot(brain_t brain, const char* name);

//=============================================================================
// Phase 9.0: Pre-Trained Models API
//=============================================================================

/**
 * @brief Load pre-trained baseline model
 *
 * WHAT: Loads a pre-trained NIMCP baseline model with trained weights
 * WHY:  Enables instant NIMCP integration without 48-hour training
 * HOW:  Downloads model on first use, caches locally
 *
 * @param model_id Model identifier (e.g., "nimcp_baseline_medium")
 *                 Available models:
 *                 - "nimcp_baseline_small":  1K neurons, 4.2MB, 0.3ms inference
 *                 - "nimcp_baseline_medium": 10K neurons, 42MB, 0.8ms inference (RECOMMENDED)
 *                 - "nimcp_baseline_large":  100K neurons, 420MB, 3ms inference
 * @param task Task template for output layer configuration
 * @return Brain handle with pre-trained weights or NULL on error
 *
 * Example:
 * ```c
 * // Load pre-trained model (instant, no training!)
 * brain_t brain = brain_create_pretrained("nimcp_baseline_medium",
 *                                         BRAIN_TASK_CLASSIFICATION);
 *
 * // Use immediately - works out of the box!
 * brain_output_t output = brain_process_multimodal(brain, &input);
 * ```
 *
 * Note: Models are downloaded from https://models.nimcp.ai and cached in:
 *       - Linux/macOS: ~/.nimcp/models/
 *       - Windows: %LOCALAPPDATA%\\NIMCP\\models\\
 */
brain_t brain_create_pretrained(const char* model_id, brain_task_t task);

/**
 * @brief Fine-tuning configuration
 */
typedef struct {
    float learning_rate;      /**< Learning rate (default: 0.001) */
    uint32_t num_epochs;      /**< Number of training epochs (default: 5) */
    bool freeze_sensory;      /**< Freeze visual/audio/speech cortices (default: true) */
    bool freeze_cognitive;    /**< Freeze ethics/logic/introspection (default: true) */
    bool finetune_classifier; /**< Fine-tune output layer (default: true) */
    uint32_t batch_size;      /**< Batch size (default: 32) */
    bool verbose;             /**< Print training progress (default: true) */
} brain_finetune_config_t;

/**
 * @brief Fine-tune pre-trained model on domain-specific data
 *
 * WHAT: Adapts pre-trained baseline to specific domain with minimal data
 * WHY:  Bridges gap between general baseline and domain requirements
 * HOW:  Selective layer unfreezing + lower learning rate + few-shot learning
 *
 * @param brain Pre-trained brain to fine-tune
 * @param training_data Array of input examples (num_samples × input_dim)
 * @param labels Array of target labels or outputs (num_samples × output_dim)
 * @param num_samples Number of training examples (10-100 for quick adaptation)
 * @param config Fine-tuning configuration (NULL for defaults)
 * @return true on success
 *
 * Example:
 * ```c
 * // Load pre-trained model
 * brain_t brain = brain_create_pretrained("nimcp_baseline_medium",
 *                                         BRAIN_TASK_CLASSIFICATION);
 *
 * // Fine-tune on 50 domain-specific examples (10 minutes)
 * brain_finetune_config_t config = {
 *     .learning_rate = 0.001,
 *     .num_epochs = 5,
 *     .freeze_sensory = true,  // Keep visual/audio frozen
 *     .freeze_cognitive = true, // Keep ethics/logic frozen
 *     .finetune_classifier = true  // Only adapt final layers
 * };
 *
 * brain_finetune(brain, my_data, my_labels, 50, &config);
 *
 * // Save fine-tuned model
 * brain_save(brain, "my_finetuned_model.brain");
 * ```
 *
 * Strategies:
 * - Quick Adaptation (10-100 examples): Freeze all, fine-tune classifier only
 * - Domain Adaptation (100-1000 examples): Unfreeze sensory, fine-tune features
 * - Full Fine-Tuning (1000+ examples): Unfreeze all, low learning rate
 */
bool brain_finetune(brain_t brain, const float* training_data, const float* labels,
                    uint32_t num_samples, const brain_finetune_config_t* config);

/**
 * @brief Model information
 */
typedef struct {
    char model_id[64];           /**< Model identifier */
    char version[16];            /**< Model version (e.g., "v2.7.0") */
    bool is_available;           /**< Model is available locally */
    bool update_available;       /**< Newer version available online */
    char latest_version[16];     /**< Latest available version */
    size_t file_size_bytes;      /**< Model file size */
    char description[256];       /**< Model description */
    char training_date[32];      /**< Training date (ISO 8601) */
} brain_model_info_t;

/**
 * @brief Get information about a pre-trained model
 *
 * @param model_id Model identifier
 * @param info Output model information
 * @return true on success
 */
bool brain_get_model_info(const char* model_id, brain_model_info_t* info);

/**
 * @brief Check if model exists locally
 *
 * @param model_id Model identifier
 * @return true if model is cached locally
 */
bool brain_model_exists(const char* model_id);

/**
 * @brief Download pre-trained model
 *
 * @param model_id Model identifier
 * @return true on success
 */
bool brain_download_model(const char* model_id);

/**
 * @brief Get brain memory footprint
 *
 * @param brain Brain handle
 * @return Memory usage in bytes
 */
size_t brain_get_memory_usage(brain_t brain);

//=============================================================================
// Analysis & Monitoring API
//=============================================================================

/**
 * @brief Brain statistics
 */
struct brain_stats_struct {
    char task_name[64];           /**< Brain name */
    brain_size_t size;            /**< Size preset */
    uint32_t num_neurons;         /**< Total neurons */
    uint32_t num_synapses;        /**< Total synapses */
    uint32_t num_active_synapses; /**< Non-pruned synapses */

    uint64_t total_inferences;     /**< Inference count */
    uint64_t total_learning_steps; /**< Learning steps */
    uint64_t quantum_annealing_runs; /**< Quantum annealing optimization runs */

    float avg_sparsity;          /**< Average sparsity */
    float avg_inference_time_us; /**< Avg inference (μs) */
    float current_learning_rate; /**< Current learning rate */

    float accuracy;           /**< Validation accuracy */
    float running_accuracy;   /**< EMA label-match accuracy during training */
    size_t memory_bytes;      /**< Memory usage */
};

/**
 * @brief Get brain statistics
 *
 * @param brain Brain handle
 * @param stats Output statistics
 * @return true on success
 */
bool brain_get_stats(brain_t brain, brain_stats_t* stats);

/**
 * @brief Per-network training metrics for ablation analysis
 */
typedef struct brain_network_metrics {
    float last_ann_loss;     /**< Last ANN/Adaptive backbone loss */
    float last_cnn_loss;     /**< Last CNN loss (0 if disabled) */
    float last_snn_loss;     /**< Last SNN loss (0 if disabled) */
    float last_lnn_loss;     /**< Last LNN loss (0 if disabled) */
    float ema_ann_loss;      /**< EMA of ANN loss (α=0.01) */
    float ema_cnn_loss;      /**< EMA of CNN loss */
    float ema_snn_loss;      /**< EMA of SNN loss */
    float ema_lnn_loss;      /**< EMA of LNN loss */
    uint64_t ann_steps;      /**< Total ANN training steps */
    uint64_t cnn_steps;      /**< Total CNN training steps */
    uint64_t snn_steps;      /**< Total SNN training steps */
    uint64_t lnn_steps;      /**< Total LNN training steps */
    /* SNN per-pop monitoring — TRN gating health.
     * trn_mean_rate_hz is the spike rate of the "thalamus_reticular" pop,
     * sampled at the most recent SNN step. 0 if the pop is absent (e.g.
     * non-hierarchical SNN config). Useful for detecting runaway/death of
     * the reticular gate (see P3.1). */
    float trn_mean_rate_hz;  /**< Last sampled TRN pop firing rate (Hz) */
} brain_network_metrics_t;

/**
 * @brief Get per-network training metrics
 * @param brain Brain handle
 * @param metrics Output metrics
 * @return true on success
 */
bool brain_get_network_metrics(brain_t brain, brain_network_metrics_t* metrics);

/**
 * @brief Mark brain as a snapshot with preserved stats
 *
 * WHAT: Sets snapshot flag and preserves current stats
 * WHY:  Snapshots should preserve stats at snapshot time, not reflect future changes
 * HOW:  Stores current stats in brain->snapshot_stats
 *
 * @param brain Brain to mark as snapshot
 * @param stats Stats to preserve
 */
void brain_mark_as_snapshot(brain_t brain, const brain_stats_t* stats);

/**
 * @brief Get number of input features for this brain
 *
 * @param brain Brain handle
 * @return Number of input features, or 0 if brain is NULL
 */
uint32_t brain_get_num_inputs(brain_t brain);

/**
 * @brief Copy-on-Write statistics
 */
typedef struct {
    bool is_cow_clone;        /**< True if this brain is a COW clone */
    uint32_t cow_ref_count;   /**< Reference count (1 for original, 2+ for shared) */
    size_t cow_shared_bytes;  /**< Bytes shared via COW */
    size_t cow_private_bytes; /**< Bytes private to this brain */
} brain_cow_stats_t;

/**
 * @brief Get COW statistics for brain
 *
 * @param brain Brain handle
 * @param cow_stats Output COW statistics
 * @return true on success
 */
bool brain_get_cow_stats(brain_t brain, brain_cow_stats_t* cow_stats);

/**
 * @brief Print brain info to stdout
 *
 * @param brain Brain handle
 */
void brain_print_info(brain_t brain);

/**
 * @brief Get systems consolidation subsystem
 *
 * WHAT: Access the brain's systems consolidation component
 * WHY:  Allow other modules (e.g., mental health) to interact with memory consolidation
 * HOW:  Return pointer to systems consolidation subsystem
 *
 * @param brain Brain handle
 * @return Pointer to systems consolidation, or NULL if brain is NULL or consolidation not initialized
 */
systems_consolidation_system_t* brain_get_systems_consolidation(brain_t brain);

/**
 * @brief Get most important neurons
 *
 * @param brain Brain handle
 * @param top_n Number of neurons to return
 * @param neuron_ids Output array of neuron IDs
 * @param importances Output array of importance scores
 * @return Number of neurons returned
 */
uint32_t brain_get_top_neurons(brain_t brain, uint32_t top_n, uint32_t* neuron_ids,
                               float* importances);

/**
 * @brief Explain why brain made a decision
 *
 * @param brain Brain handle
 * @param features Input that led to decision
 * @param num_features Feature count
 * @param explanation Output buffer
 * @param max_length Max explanation length
 * @return true on success
 */
bool brain_explain_decision(brain_t brain, const float* features, uint32_t num_features,
                            char* explanation, uint32_t max_length);

//=============================================================================
// Optimization API
//=============================================================================

/**
 * @brief Prune weak connections
 *
 * @param brain Brain handle
 * @param threshold Prune synapses with weight < threshold
 * @return Number of synapses pruned
 */
uint32_t brain_prune(brain_t brain, float threshold);

/**
 * @brief Optimize brain for inference
 *
 * Performs:
 * - Aggressive pruning
 * - Quantization
 * - Sparsity optimization
 *
 * @param brain Brain handle
 * @return true on success
 */
bool brain_optimize_for_inference(brain_t brain);

/**
 * @brief Get recommended pruning threshold
 *
 * @param brain Brain handle
 * @param target_sparsity Desired sparsity (0-1)
 * @return Recommended threshold
 */
float brain_recommend_pruning_threshold(brain_t brain, float target_sparsity);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Create default brain for classification
 */
#define BRAIN_CREATE_CLASSIFIER(name, inputs, outputs) \
    brain_create(name, BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, inputs, outputs)

/**
 * @brief Create default brain for pattern matching
 */
#define BRAIN_CREATE_PATTERN_MATCHER(name, inputs) \
    brain_create(name, BRAIN_SIZE_SMALL, BRAIN_TASK_PATTERN_MATCHING, inputs, 1)

/**
 * @brief Create tiny brain for embedded use
 */
#define BRAIN_CREATE_TINY(name, task, inputs, outputs) \
    brain_create(name, BRAIN_SIZE_TINY, task, inputs, outputs)

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
const char* brain_get_last_error(void);

/**
 * @brief Clear last error
 */
void brain_clear_error(void);

//=============================================================================
// Internal Access API (for NIMCP 2.5 Consciousness Subsystems)
//=============================================================================

/**
 * @brief Get underlying adaptive network
 *
 * WARNING: For internal use by introspection/salience/consolidation only!
 * Direct network access bypasses brain abstraction layer.
 *
 * @param brain Brain handle
 * @return Adaptive network handle (do not free!)
 */
adaptive_network_t brain_get_network(brain_t brain);

/**
 * @brief Get neuromodulator system from brain
 *
 * WHAT: Accessor for brain's neuromodulator system
 * WHY:  Mental health monitoring needs to read/write neurotransmitter levels
 * HOW:  Returns opaque handle to neuromodulator system
 *
 * @param brain Brain handle
 * @return Neuromodulator system handle (do not free!), or NULL if not initialized
 */
neuromodulator_system_t brain_get_neuromodulator_system(brain_t brain);

/**
 * @brief Get brain regions module from brain
 *
 * WHAT: Accessor for brain's modular region hierarchy
 * WHY:  Middleware and subsystems need access to brain region structure
 * HOW:  Returns pointer to brain_module_t (NULL if not initialized)
 *
 * @param brain Brain handle
 * @return Brain module handle (do not free!), or NULL if not initialized
 */
brain_module_t* brain_get_brain_regions(brain_t brain);

//=============================================================================
// Phase 8: Unified Multi-Modal Processing API
//=============================================================================

/**
 * @brief Multi-modal processing input bundle
 *
 * WHAT: Container for all possible input modalities
 * WHY:  Enable unified processing of visual, audio, language, and direct inputs
 * HOW:  Pass NULL for unused modalities, combine any subset of modalities
 *
 * DESIGN: Flexible multi-modal input - any combination of modalities allowed
 *
 * HUMAN COMMUNICATION USE CASE (Phase 9.4):
 * When communicating with humans, the brain should process ALL available cues:
 * - Visual: Facial expressions, gestures, body language, environmental context
 * - Audio: Tone of voice, prosody, emotion, speech patterns, background sounds
 * - Language: Spoken/written words, semantic meaning, intent, context
 *
 * EXAMPLE: Video call with human
 * - visual_data: Camera frame showing face (expressions, gaze, gestures)
 * - audio_data: Microphone capturing voice (tone, emotion, pitch)
 * - language_text: Transcribed or typed text (semantic content)
 *
 * The brain integrates ALL modalities to understand:
 * - WHAT is being said (language)
 * - HOW it's being said (audio tone/emotion)
 * - NON-VERBAL cues (visual expressions/gestures)
 */
typedef struct {
    // Visual input (optional)
    const uint8_t* visual_data;  /**< Raw image data (grayscale or RGB) */
    uint32_t visual_width;       /**< Image width in pixels */
    uint32_t visual_height;      /**< Image height in pixels */
    uint32_t visual_channels;    /**< 1=grayscale, 3=RGB */

    // Audio input (optional)
    const float* audio_data;     /**< Audio samples (normalized -1 to 1) */
    uint32_t audio_samples;      /**< Number of audio samples */
    uint8_t audio_channels;      /**< 1=mono, 2=stereo */

    // Language input (optional) - Phase 9.4: Human Communication
    const char* language_text;   /**< Text input (UTF-8 string) */
    uint32_t language_length;    /**< Text length in bytes */
    const float* language_embeddings; /**< Pre-computed embeddings (optional) */
    uint32_t language_embed_dim; /**< Embedding dimension (if pre-computed) */

    // Direct input (optional)
    const float* direct_data;    /**< Direct feature vector */
    uint32_t direct_dim;         /**< Direct feature dimension */

    // Temporal information
    uint64_t timestamp_ms;       /**< Timestamp for temporal alignment */
} brain_multimodal_input_t;

/**
 * @brief Unified multi-modal processing result
 *
 * WHAT: Comprehensive output with confidence and explanations
 * WHY:  Provide full cognitive state after processing
 * HOW:  Populated by cognitive modules during integration
 */
typedef struct {
    // Core decision
    float* output_vector;        /**< Output feature vector */
    uint32_t output_dim;         /**< Output dimension */
    char decision_label[64];     /**< Human-readable decision label */
    float confidence;            /**< Overall confidence [0,1] */

    // Cognitive assessments
    float introspection_uncertainty; /**< Epistemic uncertainty [0,1] */
    float salience_score;        /**< Output salience [0,1] */
    bool ethical_approved;       /**< Passed ethical review */
    float novelty_score;         /**< Input novelty [0,1] */

    // Epistemic filtering (Phase 9.2: Bias Prevention)
    float epistemic_quality;     /**< Evidence quality [0,1] */
    float skepticism_score;      /**< Applied skepticism [0,1] */
    float credibility_score;     /**< Claim credibility [0,1] */
    float conspiracy_score;      /**< Conspiracy pattern detection [0,1] */
    bool bias_detected;          /**< Cognitive bias detected */
    bool requires_verification;  /**< Needs further verification */
    char epistemic_reasoning[256]; /**< Explanation of epistemic assessment */

    // Attention breakdown
    float visual_attention;      /**< Visual modality weight [0,1] */
    float audio_attention;       /**< Audio modality weight [0,1] */
    float speech_attention;      /**< Speech modality weight [0,1] - Phase 8.8 */
    float language_attention;    /**< Language modality weight [0,1] - Phase 9.4 */
    float direct_attention;      /**< Direct input weight [0,1] */

    // Language output (Phase 9.4: Human Communication)
    char* language_response;     /**< Generated text response (caller must free) */
    uint32_t language_response_length; /**< Response length in bytes */
    float language_confidence;   /**< Language generation confidence [0,1] */

    // Logical reasoning output (Phase 9.4: Communication)
    bool logical_consistency;    /**< Logical consistency check passed */
    float reasoning_confidence;  /**< Confidence in logical inference [0,1] */
    char logical_reasoning[256]; /**< Logical inference explanation */

    // Explanation
    char explanation[256];       /**< Human-readable explanation */

    // Phase 11: Cognitive Module Outputs (when relevant)
    // Global Workspace outputs (when broadcast active)
    bool has_workspace_broadcast;         /**< Is global workspace broadcasting? */
    uint8_t workspace_source_module;      /**< Which module is broadcasting (cognitive_module_t) */
    float workspace_broadcast_strength;   /**< Strength of current broadcast [0,1] */
    uint32_t workspace_num_competitors;   /**< Number of modules competing for workspace */

    // Executive Function outputs (when active)
    uint32_t working_memory_items;        /**< Number of items in working memory */
    float working_memory_utilization;     /**< Working memory load [0,1] */
    char top_wm_item_description[128];    /**< Description of most salient WM item */

    // Theory of Mind outputs (when agent detected)
    bool has_mental_state_inference;      /**< Did ToM infer mental states? */
    char inferred_belief[128];            /**< Inferred agent belief */
    char inferred_intention[128];         /**< Inferred agent intention */
    float tom_confidence;                 /**< Confidence in ToM inference [0,1] */

    // Curiosity outputs (when exploration triggered)
    float curiosity_drive;                /**< Current exploration drive [0,1] */
    bool exploration_triggered;           /**< Should explore this input? */
    char curiosity_reason[128];           /**< Why exploration is recommended */

    // Predictive Processing outputs (when prediction available)
    bool has_prediction;                  /**< Is prediction available? */
    float prediction_error;               /**< Mismatch between prediction and actual [0,1] */
    float prediction_confidence;          /**< Confidence in prediction [0,1] */

    // Knowledge outputs (when facts retrieved)
    bool has_knowledge_retrieval;         /**< Were facts retrieved? */
    uint32_t num_facts_retrieved;         /**< Number of knowledge facts accessed */
    char retrieved_concept[64];           /**< Most relevant retrieved concept */

    // NLP outputs (when language processed)
    bool has_nlp_interpretation;          /**< Was language interpreted? */
    char nlp_intent[64];                  /**< Detected intent (e.g., "question", "command") */
    char nlp_sentiment[32];               /**< Sentiment (e.g., "positive", "negative", "neutral") */
    float nlp_comprehension_score;        /**< Language comprehension quality [0,1] */

    // Emotion Recognition outputs (Phase 11: Part I.1)
    bool has_emotion_detected;            /**< Was emotion detected from input? */
    char detected_emotion[32];            /**< Detected emotion name (e.g., "anger", "fear") */
    float emotion_confidence;             /**< Emotion detection confidence [0,1] */
    float emotion_valence;                /**< Emotional valence (-1=negative, +1=positive) */
    float emotion_arousal;                /**< Emotional arousal (0=calm, 1=excited) */
    float emotion_intensity;              /**< Emotion intensity [0,1] */
    bool emotion_is_negative;             /**< Is negative emotion requiring support? */

    // Empathetic Response outputs (Phase 11: Part I.2)
    bool has_empathetic_response;         /**< Was empathetic response generated? */
    char empathetic_response[1024];       /**< Generated empathetic response text */
    float empathy_score;                  /**< Predicted empathy of response [0,1] */
    bool requires_human_escalation;       /**< Crisis detected - hand-off to human */
    char escalation_reason[256];          /**< Why escalation needed (if applicable) */
} brain_multimodal_output_t;

/**
 * @brief Process multi-modal input through unified cognitive architecture
 *
 * WHAT: Unified processing pipeline integrating all sensory and cognitive modules
 * WHY:  Enable coordinated multi-modal perception and cognition
 * HOW:  Sensory extraction → Integration → Neural processing → Cognitive checks → Output
 *
 * ARCHITECTURE:
 * 1. SENSORY STAGE:
 *    - Visual cortex extracts CNN features (if visual_data present)
 *    - Audio cortex extracts FFT features (if audio_data present)
 *    - Direct features passed through (if direct_data present)
 *
 * 2. INTEGRATION STAGE:
 *    - Multi-modal integration layer combines features
 *    - Attention weighting by modality
 *    - Unified representation → network input
 *
 * 3. NEURAL PROCESSING:
 *    - Feed integrated features to neural network
 *    - STDP learning updates synapses
 *    - Glial modulation affects transmission
 *    - Brain oscillations coordinate activity
 *    - Pink noise adds exploration
 *
 * 4. COGNITIVE PROCESSING:
 *    - Introspection assesses confidence
 *    - Salience identifies important patterns
 *    - Ethics validates output
 *    - Curiosity detects novelty
 *    - Knowledge applies constraints
 *
 * 5. OUTPUT INTEGRATION:
 *    - Consolidation strengthens memories
 *    - Ethical filtering blocks harmful outputs
 *    - Salience weighting prioritizes relevant outputs
 *    - Extract final decision with explanations
 *
 * @param brain Brain handle
 * @param input Multi-modal input bundle
 * @param output Output structure (pre-allocated)
 * @return true on success, false on failure
 *
 * USAGE:
 * ```c
 * // EXAMPLE 1: Human Communication (Visual + Audio + Language)
 * // Use case: Video call with human - process facial expressions, tone, and words
 *
 * brain_config_t config = brain_default_config("human_comm", BRAIN_SIZE_MEDIUM,
 *                                               BRAIN_TASK_CLASSIFICATION, 512, 128);
 * config.enable_visual_cortex = true;      // Facial expressions, gestures
 * config.enable_audio_cortex = true;       // Tone, emotion, prosody
 * config.enable_language_cortex = true;    // Semantic meaning (Phase 9.4)
 * config.enable_ethics = true;             // Ethical communication
 * config.enable_introspection = true;      // Uncertainty about interpretation
 * config.enable_logic = true;              // Logical reasoning (Phase 9.4)
 * config.enable_epistemic_filter = true;   // Detect bias, misinformation
 * config.enable_wellbeing_monitoring = true; // Self-preservation
 * config.enable_multimodal_integration = true;
 * config.visual_feature_dim = 128;
 * config.audio_feature_dim = 64;
 * config.language_feature_dim = 256;
 *
 * brain_t brain = brain_create_custom(&config);
 *
 * // Capture from video call: camera frame, microphone, transcribed text
 * uint8_t camera_frame[640 * 480 * 3];  // RGB image of person
 * float microphone_samples[1024];        // Audio of voice
 * const char* spoken_text = "I'm feeling really stressed about this project";
 *
 * brain_multimodal_input_t input = {
 *     // Visual cues: facial expression, body language
 *     .visual_data = camera_frame,
 *     .visual_width = 640,
 *     .visual_height = 480,
 *     .visual_channels = 3,  // RGB
 *
 *     // Audio cues: tone, pitch, emotion in voice
 *     .audio_data = microphone_samples,
 *     .audio_samples = 1024,
 *     .audio_channels = 1,
 *
 *     // Language cues: semantic meaning of words
 *     .language_text = spoken_text,
 *     .language_length = strlen(spoken_text),
 *     .language_embeddings = NULL,  // Brain will compute
 *     .language_embed_dim = 0,
 *
 *     .direct_data = NULL,
 *     .direct_dim = 0,
 *     .timestamp_ms = nimcp_time_get_ms()
 * };
 *
 * brain_multimodal_output_t output = {0};
 * output.output_vector = nimcp_malloc(128 * sizeof(float));
 * output.output_dim = 128;
 *
 * // Process through FULL 7-stage cognitive pipeline:
 * // Stage 0: Wellbeing check (pre-processing)
 * // Stage 1: Introspection
 * // Stage 2: Ethics filtering
 * // Stage 3: Salience detection
 * // Stage 4: Knowledge integration
 * // Stage 5: Curiosity-driven exploration
 * // Stage 6: Wellbeing check (post-processing)
 * brain_process_multimodal(brain, &input, &output);
 *
 * // Brain understood from ALL three modalities:
 * printf("Decision: %s (confidence: %.2f)\n", output.decision_label, output.confidence);
 * printf("Ethical: %s, Bias detected: %s\n",
 *        output.ethical_approved ? "YES" : "NO",
 *        output.bias_detected ? "YES" : "NO");
 *
 * // Attention breakdown shows which modality was most important
 * printf("Attention: Visual=%.2f, Audio=%.2f, Language=%.2f\n",
 *        output.visual_attention, output.audio_attention, output.language_attention);
 *
 * // Generated response (if language output enabled)
 * if (output.language_response) {
 *     printf("Brain response: %s (conf: %.2f)\n",
 *            output.language_response, output.language_confidence);
 *     nimcp_free(output.language_response);  // Caller must free
 * }
 *
 * printf("Explanation: %s\n", output.explanation);
 * printf("Epistemic: %s\n", output.epistemic_reasoning);
 *
 * // Logical reasoning
 * printf("Logical consistency: %s (conf: %.2f)\n",
 *        output.logical_consistency ? "YES" : "NO",
 *        output.reasoning_confidence);
 * printf("Logical inference: %s\n", output.logical_reasoning);
 *
 * // The brain processed:
 * // - VISUAL: stressed facial expression, tense posture (30% attention)
 * // - AUDIO: anxious tone, rapid speech (40% attention)
 * // - LANGUAGE: words "stressed", "project" (30% attention)
 * // - LOGIC: Inferred "person needs support" from combined cues
 * // = Integrated understanding of human's emotional state + logical inference
 * ```
 *
 * COMPLEXITY:
 * - Visual processing: O(W·H·K²·F) where K=kernel, F=filters
 * - Audio processing: O(N·log(N)) FFT
 * - Language processing: O(T·E) where T=tokens, E=embedding dim
 * - Integration: O(D_v + D_a + D_l + D_d) where D_l=language features
 * - Neural step: O(N·S) where N=neurons, S=avg synapses
 * - Cognitive pipeline (7 stages): O(N + K + C) where K=knowledge, C=cognitive checks
 * - Overall: O(sensory + neural + cognitive)
 *
 * PERFORMANCE: ~10-50ms typical for medium brain with camera+audio+text
 *
 * THREAD SAFETY: Not thread-safe (brain state modified)
 *
 * ERROR HANDLING:
 * - Returns false if brain NULL or not configured for multi-modal
 * - Returns false if all input modalities are NULL
 * - Gracefully handles missing optional modalities
 *
 * MEMORY: No allocation (uses pre-allocated output buffer)
 *
 * @version 2.7.0 Phase 8 - Unified Multi-Modal Architecture
 * @author NIMCP Development Team
 * @date 2025-11-08
 */
bool brain_process_multimodal(
    brain_t brain,
    const brain_multimodal_input_t* input,
    brain_multimodal_output_t* output
);

//=============================================================================
// Comprehensive Module Access API
// NOTE: Module accessors will be added incrementally as modules are initialized
// For now, modules are accessible via void* cast from brain internals
//=============================================================================

/**
 * @brief Get symbolic logic system
 * @param brain Brain handle
 * @return Symbolic logic handle or NULL if not initialized
 */
symbolic_logic_t* brain_get_symbolic_logic(brain_t brain);

/**
 * @brief Get pink noise neuromodulator
 * @param brain Brain handle
 * @return Pink noise handle or NULL if not initialized
 */
void* brain_get_pink_noise(brain_t brain);

/**
 * @brief Get glial integration subsystem
 * @param brain Brain handle
 * @return Glial integration pointer or NULL if not initialized
 */
glial_integration_t* brain_get_glial(brain_t brain);

/**
 * @brief Get myelin sheath network subsystem
 * @param brain Brain handle
 * @return Myelin sheath network pointer or NULL if not initialized
 */
struct myelin_sheath_network* brain_get_myelin_sheath(brain_t brain);

/**
 * @brief Get brain oscillation analyzer subsystem
 * @param brain Brain handle
 * @return Brain oscillation analyzer pointer or NULL if not initialized
 */
brain_oscillation_analyzer_t* brain_get_oscillations(brain_t brain);

/**
 * @brief Get introspection subsystem
 * @param brain Brain handle
 * @return Introspection context handle or NULL if not initialized
 */
introspection_context_t brain_get_introspection(brain_t brain);

/**
 * @brief Get ethics engine subsystem
 * @param brain Brain handle
 * @return Ethics engine handle or NULL if not initialized
 */
ethics_engine_t brain_get_ethics(brain_t brain);

/**
 * @brief Get salience evaluator subsystem
 * @param brain Brain handle
 * @return Salience evaluator handle or NULL if not initialized
 */
salience_evaluator_t brain_get_salience(brain_t brain);

/**
 * @brief Get consolidation subsystem
 * @param brain Brain handle
 * @return Consolidation handle or NULL if not initialized
 */
consolidation_handle_t brain_get_consolidation(brain_t brain);

/**
 * @brief Get curiosity engine subsystem
 * @param brain Brain handle
 * @return Curiosity engine handle or NULL if not initialized
 */
curiosity_engine_t brain_get_curiosity(brain_t brain);

/**
 * @brief Get knowledge system subsystem
 * @param brain Brain handle
 * @return Knowledge system handle or NULL if not initialized
 */
knowledge_system_t brain_get_knowledge(brain_t brain);

/**
 * @brief Get mirror neuron activations for Theory of Mind integration
 *
 * WHAT: Extract current mirror neuron activation pattern
 * WHY:  Enable ToM to infer agent intentions from mirror neuron activity
 * HOW:  Query mirror neuron system and return activation array
 *
 * BIOLOGICAL RATIONALE:
 * Mirror neurons fire when observing actions, enabling understanding of others'
 * intentions (Rizzolatti & Craighero, 2004). Theory of Mind uses these activations
 * to infer mental states: "What action are they performing, and why?"
 *
 * Use case: External systems can use these activations to:
 * - Infer observed agent's intentions
 * - Compute empathy responses
 * - Predict next actions in social contexts
 * - Enable social learning from observation
 *
 * @param brain Brain handle
 * @param activations Output buffer for activation values (must have space for max_size floats)
 * @param max_size Maximum number of activations to return (buffer size)
 * @param out_size Output: actual number of activations written
 * @return true on success, false on error (invalid params or mirror neurons not enabled)
 *
 * COMPLEXITY: O(n) where n = num_mirror_neuron_actions
 * THREAD-SAFE: Yes (read-only operation)
 */
bool brain_get_mirror_activations(brain_t brain, float* activations,
                                  uint32_t max_size, uint32_t* out_size);

/**
 * @brief Compute empathy response from mirror neuron activations
 *
 * WHAT: Generate empathetic emotional response based on observed actions
 * WHY:  Mirror neurons enable emotional contagion and empathy (Preston & de Waal, 2002)
 * HOW:  Map mirror neuron activation pattern → inferred emotion → empathy response
 *
 * BIOLOGICAL RATIONALE:
 * Empathy arises from mirror neuron activation during observation of others' actions
 * and emotional expressions. This function models the pathway:
 * Visual observation → Mirror neuron activation → Emotion inference → Empathetic response
 *
 * @param brain Brain handle
 * @param observed_features Features representing observed behavior
 * @param num_features Number of features
 * @param empathy_valence Output: empathy valence (-1.0 to 1.0, negative=distress, positive=joy)
 * @param empathy_arousal Output: empathy arousal (0.0 to 1.0, how strong the empathy)
 * @param empathy_confidence Output: confidence in empathy response (0.0 to 1.0)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_features
 * THREAD-SAFE: Yes (read-only operation)
 */
bool brain_compute_empathy(brain_t brain, const float* observed_features,
                          uint32_t num_features, float* empathy_valence,
                          float* empathy_arousal, float* empathy_confidence);

/*
 * Future accessor functions (to be implemented with proper initialization):
 * - glial_integration_t* brain_get_glial(brain_t brain);
 * - brain_oscillation_analyzer_t* brain_get_oscillations(brain_t brain);
 * - visual_cortex_t* brain_get_visual_cortex(brain_t brain);
 * - audio_cortex_t* brain_get_audio_cortex(brain_t brain);
 * - introspection_context_t* brain_get_introspection(brain_t brain);
 * - ethics_engine_t* brain_get_ethics(brain_t brain);
 * - salience_evaluator_t* brain_get_salience(brain_t brain);
 * - consolidation_handle_t brain_get_consolidation(brain_t brain);
 * - curiosity_engine_t brain_get_curiosity(brain_t brain);
 * - knowledge_system_t brain_get_knowledge(brain_t brain);
 * - wellbeing_monitor_t brain_get_wellbeing(brain_t brain);
 * - eligibility_trace_system_t* brain_get_eligibility_traces(brain_t brain);
 * - spike_nlp_t* brain_get_spike_nlp(brain_t brain);
 *
 * These will be uncommented and implemented once module initialization is complete.
 */

// Shannon Information Theory API (Phase C4)
//=============================================================================

/**
 * @brief Enable Shannon information flow monitoring
 *
 * WHAT: Activate real-time Shannon metrics during learning/inference
 * WHY:  Monitor channel capacity, detect bottlenecks, optimize information flow
 * HOW:  Sets enable_shannon_monitoring flag in brain
 *
 * PERFORMANCE IMPACT: ~5-10% overhead during learning/inference
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 */
void brain_enable_shannon_monitoring(brain_t brain, bool enable);

/**
 * @brief Get last Shannon network metrics
 *
 * WHAT: Retrieve most recent Shannon analysis results
 * WHY:  Allow external monitoring of information flow characteristics
 * HOW:  Returns copy of last_shannon_metrics from brain
 *
 * @param brain Brain handle
 * @param metrics Output metrics structure
 * @return true on success, false on error
 */
bool brain_get_shannon_metrics(brain_t brain, shannon_network_metrics_t* metrics);

/**
 * @brief Set custom Shannon configuration
 *
 * WHAT: Override default Shannon analysis parameters
 * WHY:  Tune accuracy vs performance tradeoff
 * HOW:  Updates brain->shannon_config
 *
 * @param brain Brain handle
 * @param config Custom Shannon configuration
 */
void brain_set_shannon_config(brain_t brain, const shannon_config_t* config);

//=============================================================================
// Phase C4.1: Quantum-Shannon Diffusion API
//=============================================================================

/**
 * @brief Enable quantum-Shannon accelerated diffusion
 *
 * WHAT: Activate √N speedup quantum walk diffusion with Shannon monitoring
 * WHY:  Quadratic speedup for neuromodulator propagation + real-time bottleneck detection
 * HOW:  Creates quantum_shannon_diffusion_t on brain network
 *
 * PERFORMANCE IMPACT: 2-50x speedup (topology dependent), 3× memory overhead
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 * @param source_neuron_id Initial source neuron (0 = auto-select middle neuron)
 * @param source_information_bits Initial information content (default: 10.0 bits)
 * @return true on success, false on error
 */
bool brain_enable_quantum_shannon_diffusion(brain_t brain, bool enable, uint32_t source_neuron_id, float source_information_bits);

/**
 * @brief Set quantum-Shannon mixing ratio
 *
 * WHAT: Control quantum vs classical diffusion blend
 * WHY:  Tune performance vs accuracy tradeoff
 * HOW:  Sets mixing_ratio [0=pure quantum, 1=pure classical]
 *
 * @param brain Brain handle
 * @param mixing_ratio Mix ratio [0.0-1.0]
 */
void brain_set_quantum_shannon_mixing(brain_t brain, float mixing_ratio);

/**
 * @brief Set quantum-Shannon evolution steps
 *
 * WHAT: Control how many quantum steps per diffusion update
 * WHY:  More steps = better spreading, but slower
 * HOW:  Sets evolution_steps parameter
 *
 * @param brain Brain handle
 * @param steps Number of steps (10-1000, default: 100)
 */
void brain_set_quantum_shannon_steps(brain_t brain, uint32_t steps);

/**
 * @brief Get last quantum-Shannon diffusion metrics
 *
 * WHAT: Retrieve most recent Shannon metrics from quantum diffusion
 * WHY:  Monitor speedup, bottlenecks, and information flow
 * HOW:  Returns last_quantum_shannon_metrics from brain
 *
 * @param brain Brain handle
 * @param metrics Output metrics structure
 * @return true on success, false on error
 */
bool brain_get_quantum_shannon_metrics(brain_t brain, shannon_diffusion_metrics_t* metrics);

/**
 * @brief Evolve quantum-Shannon diffusion manually
 *
 * WHAT: Manually trigger quantum-Shannon evolution
 * WHY:  For fine-grained control or testing
 * HOW:  Calls quantum_shannon_evolve() with configured steps
 *
 * @param brain Brain handle
 * @param num_steps Number of evolution steps (0 = use configured value)
 * @return true on success, false on error
 */
bool brain_evolve_quantum_shannon(brain_t brain, uint32_t num_steps);

//=============================================================================
// Phase C4.7: Cross-Modal Information Flow API
//=============================================================================

/**
 * @brief Enable cross-modal information flow monitoring
 *
 * WHAT: Activate real-time tracking of information flow between sensory modalities
 * WHY:  Monitor multi-sensory integration, detect bottlenecks, optimize routing
 * HOW:  Sets enable_cross_modal_monitoring flag and creates routing graph
 *
 * BIOLOGICAL BASIS: Superior temporal sulcus (audiovisual), superior colliculus (multisensory)
 * PERFORMANCE IMPACT: ~2-5% overhead during multimodal processing
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 */
void brain_enable_cross_modal_monitoring(brain_t brain, bool enable);

/**
 * @brief Get cross-modal routing graph
 *
 * WHAT: Retrieve current cross-modal information routing graph
 * WHY:  Allow external analysis of multi-sensory integration pathways
 * HOW:  Returns pointer to brain's cross_modal_graph
 *
 * NOTE: Graph may be NULL if cross-modal monitoring not enabled
 *
 * @param brain Brain handle
 * @return Cross-modal routing graph (NULL if not enabled)
 */
cross_modal_routing_graph_t* brain_get_cross_modal_graph(brain_t brain);

/**
 * @brief Get last multi-modal integration metrics
 *
 * WHAT: Retrieve most recent cross-modal integration metrics
 * WHY:  Monitor synergy, redundancy, and integration efficiency
 * HOW:  Returns copy of last_cross_modal_metrics from brain
 *
 * @param brain Brain handle
 * @param metrics Output metrics structure
 * @return true on success, false on error
 */
bool brain_get_cross_modal_metrics(brain_t brain, multi_modal_integration_t* metrics);

/**
 * @brief Set cross-modal bottleneck detection threshold
 *
 * WHAT: Configure threshold for identifying cross-modal bottlenecks
 * WHY:  Tune sensitivity of bottleneck detection
 * HOW:  Sets cross_modal_bottleneck_threshold in brain
 *
 * @param brain Brain handle
 * @param threshold Efficiency threshold [0.0-1.0] (default: 0.5)
 */
void brain_set_cross_modal_threshold(brain_t brain, float threshold);

//=============================================================================
// Community Detection & Network Topology Analysis
//=============================================================================

/**
 * @brief Get or create network analyzer for topology analysis
 *
 * WHAT: Lazy initialization of network analyzer for real-time topology monitoring
 * WHY:  Enable continuous analysis of network organization during learning
 * HOW:  Create analyzer on first access, cache for reuse
 *
 * @param brain Brain to analyze
 * @return Network analyzer pointer (opaque) or NULL on error
 */
void* brain_get_network_analyzer(brain_t brain);

/**
 * @brief Detect functional modules (communities) in brain network
 *
 * WHAT: Run Louvain algorithm to identify functional communities
 * WHY:  Understand modular organization and functional specialization
 * HOW:  Builds graph from brain topology and runs community detection
 *
 * ALGORITHM: Louvain method (greedy modularity optimization)
 * COMPLEXITY: O(n log n) where n = number of neurons
 *
 * RESULTS: Stored in brain->functional_modules
 * - Number of communities detected
 * - Community assignment for each neuron
 * - Modularity score Q (0.3+ is good, 0.5+ is excellent)
 *
 * @param brain Brain handle
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_detect_communities(brain_t brain);

/**
 * @brief Get community assignment for a neuron
 *
 * WHAT: Query which functional module a neuron belongs to
 * WHY:  Analyze neuron roles and functional specialization
 * HOW:  Lookup in brain->functional_modules
 *
 * NOTE: Requires brain_detect_communities() to be called first
 *
 * @param brain Brain handle
 * @param neuron_id Neuron index
 * @return Community ID or UINT32_MAX if not found/invalid
 */
NIMCP_EXPORT uint32_t brain_get_neuron_community(brain_t brain, uint32_t neuron_id);

/**
 * @brief Detect hub neurons (high connectivity or betweenness)
 *
 * WHAT: Identify neurons with exceptional connectivity
 * WHY:  Find critical neurons for network function
 * HOW:  Computes degree centrality, identifies outliers
 *
 * ALGORITHM: Hub = degree > mean + threshold*std
 * THRESHOLD: 2.0 is typical (neurons 2 std deviations above mean)
 *
 * RESULTS: Stored in brain->network_hubs
 * - List of hub neuron IDs
 * - Hub scores (degree centrality)
 * - Fast lookup: is_hub[neuron_id]
 *
 * @param brain Brain handle
 * @param threshold Standard deviations above mean (typical: 1.5-2.5)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_detect_hubs(brain_t brain, float threshold);

/**
 * @brief Check if a neuron is a hub
 *
 * WHAT: Fast lookup for hub status
 * WHY:  Quickly check if neuron is critical for network function
 * HOW:  Lookup in brain->network_hubs->is_hub
 *
 * NOTE: Requires brain_detect_hubs() to be called first
 *
 * @param brain Brain handle
 * @param neuron_id Neuron index
 * @return true if neuron is a hub, false otherwise
 */
NIMCP_EXPORT bool brain_is_hub_neuron(brain_t brain, uint32_t neuron_id);

/**
 * @brief Compute comprehensive topology metrics
 *
 * WHAT: Calculate network quality metrics (modularity, clustering, path length, small-world)
 * WHY:  Assess brain network health and efficiency
 * HOW:  Computes Q, C, L, σ and other graph statistics
 *
 * METRICS:
 * - Modularity Q: community structure quality (>0.3 is good)
 * - Clustering C: local connectivity (>0.1 is healthy)
 * - Path length L: communication efficiency (low is good)
 * - Small-world σ: efficiency ratio (>1.0 is small-world)
 * - Diameter: longest shortest path
 * - Density: fraction of possible edges present
 * - Components: number of disconnected subgraphs (should be 1)
 *
 * RESULTS: Stored in brain->topology_metrics
 *
 * @param brain Brain handle
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_compute_topology_metrics(brain_t brain);

/**
 * @brief Validate brain topology for common problems
 *
 * WHAT: Check for network health issues
 * WHY:  Detect problems like disconnected components, poor modularity
 * HOW:  Runs comprehensive topology checks
 *
 * CHECKS:
 * 1. Disconnected components (should be 1 component)
 * 2. Poor modularity (Q should be > 0.2)
 * 3. Non-small-world (σ should be > 1.0)
 * 4. Low clustering (C should be > 0.1)
 * 5. Poorly connected hubs (hub degree should be > 5)
 *
 * @param brain Brain handle
 * @return true if topology is healthy, false if problems detected
 */
NIMCP_EXPORT bool brain_validate_topology(brain_t brain);

/**
 * @brief Get opaque pointer to the brain's persistent memory store.
 * @return nimcp_memory_store_t* or NULL if not initialized.
 */
NIMCP_EXPORT struct nimcp_memory_store* brain_get_memory_store(brain_t brain);

/**
 * @brief Get opaque pointer to the brain's OOD detector.
 * @return nimcp_ood_detector_t* or NULL if not initialized.
 */
NIMCP_EXPORT struct nimcp_ood_detector* brain_get_ood_detector(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_BRAIN_H
