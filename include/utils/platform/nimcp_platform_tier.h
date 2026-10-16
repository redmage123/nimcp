//=============================================================================
// nimcp_platform_tier.h - Platform Tier System (Portia Spider Foundation)
//=============================================================================
/**
 * @file nimcp_platform_tier.h
 * @brief Platform tier detection and configuration for constrained devices
 *
 * WHAT: Automatic platform classification and tier-specific configuration
 * WHY:  Enable NIMCP to run on diverse hardware (server → IoT/MCU)
 * HOW:  Detect system resources and apply tier-appropriate constraints
 *
 * PORTIA SPIDER FOUNDATION:
 * Named after Portia fimbriata, a jumping spider with extraordinary cognitive
 * abilities despite having only ~600,000 neurons and operating on minimal
 * energy. Just as Portia adapts its hunting strategies to available resources,
 * NIMCP adapts its architecture to platform constraints.
 *
 * DESIGN PHILOSOPHY:
 * Different platforms have vastly different resources:
 * - Desktop/Server: 8+ cores, 16GB+ RAM, GPU acceleration
 * - Laptop/Tablet: 4+ cores, 4GB+ RAM, modest compute
 * - Drone/Phone: 2-4 cores, 1-4GB RAM, power-constrained
 * - IoT/MCU: 1-2 cores, 64-256MB RAM, extreme constraints
 *
 * This system automatically detects platform capabilities and configures
 * NIMCP components (neuron counts, cortex resolution, cognitive modules)
 * to match available resources.
 *
 * TIER CLASSIFICATION:
 * - FULL: High-end workstations, servers (≥8 cores, ≥8GB RAM)
 * - MEDIUM: Laptops, tablets, dev boards (≥4 cores, ≥2GB RAM)
 * - CONSTRAINED: Phones, drones, edge devices (≥2 cores, ≥256MB RAM)
 * - MINIMAL: IoT, microcontrollers (≥1 core, ≥64MB RAM)
 *
 * COGNITIVE ADAPTATION:
 * Each tier enables/disables cognitive modules based on compute budget:
 * - FULL: All modules (curiosity, emotions, executive, meta-learning)
 * - MEDIUM: Core cognition (working memory, attention, emotions)
 * - CONSTRAINED: Essential modules (basic attention, simple memory)
 * - MINIMAL: Bare essentials (reactive processing only)
 *
 * INTEGRATION:
 * Brain creation → platform_tier_detect() → get tier config → apply limits
 * Visual cortex → tier config → set resolution/filters
 * Audio cortex → tier config → set sample rate/mel filters
 * Cognitive systems → tier config → enable/disable modules
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 2.8.0
 */

#ifndef NIMCP_PLATFORM_TIER_H
#define NIMCP_PLATFORM_TIER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/platform/nimcp_system_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Platform Tier Preprocessor Constants
//=============================================================================
// WHAT: Preprocessor-visible tier values for #if directives
// WHY:  C preprocessor #if cannot see enum values (treats them as 0)
//       This caused ALL tier-conditional buffer sizes to use MINIMAL values,
//       leading to stack buffer overflows (e.g., 32-byte label buffer + 63-byte strncpy)
// HOW:  Define macros with same values as the enum, used in nimcp_tier_optimization.h

#define PLATFORM_TIER_BASIC_VALUE        0
#define PLATFORM_TIER_MINIMAL_VALUE      1
#define PLATFORM_TIER_CONSTRAINED_VALUE  2
#define PLATFORM_TIER_MEDIUM_VALUE       3
#define PLATFORM_TIER_FULL_VALUE         4
#define PLATFORM_TIER_NEUROMORPHIC_VALUE 5
#define PLATFORM_TIER_QUANTUM_VALUE      6

//=============================================================================
// Platform Tier Enumeration
//=============================================================================

/**
 * @brief Platform tier classification
 *
 * WHAT: Seven-tier system from ultra-constrained to specialized hardware
 * WHY:  Balance functionality vs resource constraints, support specialized hardware
 * HOW:  Detect RAM/cores and classify into appropriate tier, or explicitly set for specialized hardware
 *
 * TIER HIERARCHY:
 * - Standard tiers: BASIC < MINIMAL < CONSTRAINED < MEDIUM < FULL
 * - Specialized tiers: NEUROMORPHIC, QUANTUM (orthogonal to standard tiers)
 */
typedef enum {
    /**
     * PLATFORM_TIER_BASIC - Arduino/ESP32/ultra-constrained MCU
     * Hardware: 1 core, ≤32MB RAM, limited FPU
     * Use cases: Sensor nodes, simple actuators, edge triggers
     * Cognitive: None - pure reactive processing only
     * Biological: Like Portia's simplest reflexes
     */
    PLATFORM_TIER_BASIC = 0,

    /**
     * PLATFORM_TIER_MINIMAL - IoT/MCU/ultra-constrained
     * Hardware: ≥1 core, ≥64MB RAM
     * Use cases: Sensors, actuators, minimal inference
     * Cognitive: Reactive processing only (no complex cognition)
     */
    PLATFORM_TIER_MINIMAL = 1,

    /**
     * PLATFORM_TIER_CONSTRAINED - Phone/drone/embedded
     * Hardware: ≥2 cores, ≥256MB RAM
     * Use cases: Mobile robotics, drones, IoT gateways
     * Cognitive: Essential modules only (basic attention, simple memory)
     */
    PLATFORM_TIER_CONSTRAINED = 2,

    /**
     * PLATFORM_TIER_MEDIUM - Laptop/tablet/dev board
     * Hardware: ≥4 cores, ≥2GB RAM
     * Use cases: Development, inference, edge AI
     * Cognitive: Core modules (working memory, attention, emotions)
     */
    PLATFORM_TIER_MEDIUM = 3,

    /**
     * PLATFORM_TIER_FULL - High-end desktop/server
     * Hardware: ≥8 cores, ≥8GB RAM
     * Use cases: Research, development, training
     * Cognitive: All modules enabled
     */
    PLATFORM_TIER_FULL = 4,

    /**
     * PLATFORM_TIER_NEUROMORPHIC - Spike-based neuromorphic hardware
     * Hardware: Loihi, SpiNNaker, BrainScaleS, TrueNorth
     * Characteristics:
     *   - Native spike routing (no software simulation)
     *   - Event-driven processing (ultra-low power)
     *   - Massive parallelism (millions of neurons)
     *   - On-chip plasticity (STDP hardware)
     * Use cases: Real-time SNN inference, low-power edge AI, robotics
     * Configuration is separate - not auto-detected
     */
    PLATFORM_TIER_NEUROMORPHIC = 5,

    /**
     * PLATFORM_TIER_QUANTUM - Hybrid quantum-classical systems
     * Hardware: NISQ devices, quantum annealers, quantum simulators
     * Characteristics:
     *   - Quantum superposition for parallel state evaluation
     *   - Entanglement for correlated decisions
     *   - Quantum tunneling for optimization landscapes
     *   - Classical fallback for non-quantum operations
     * Use cases: Optimization, pattern matching, quantum consensus
     * Configuration is separate - not auto-detected
     */
    PLATFORM_TIER_QUANTUM = 6,

    PLATFORM_TIER_COUNT = 7  /**< Number of tiers (including specialized) */
} platform_tier_t;

/**
 * @brief Neuromorphic hardware type
 */
typedef enum {
    NEUROMORPHIC_HW_GENERIC = 0,    /**< Generic neuromorphic simulation */
    NEUROMORPHIC_HW_LOIHI,          /**< Intel Loihi (1/2) */
    NEUROMORPHIC_HW_SPINNAKER,      /**< SpiNNaker (Manchester) */
    NEUROMORPHIC_HW_BRAINSCALES,    /**< BrainScaleS (Heidelberg) */
    NEUROMORPHIC_HW_TRUENORTH,      /**< IBM TrueNorth */
    NEUROMORPHIC_HW_DYNAPSE,        /**< Dynapse (INI Zurich) */
    NEUROMORPHIC_HW_AKIDA,          /**< BrainChip Akida */
    NEUROMORPHIC_HW_COUNT
} neuromorphic_hardware_t;

/**
 * @brief Quantum backend type
 */
typedef enum {
    QUANTUM_BACKEND_SIMULATOR = 0,  /**< Classical quantum simulator */
    QUANTUM_BACKEND_IBMQ,           /**< IBM Quantum */
    QUANTUM_BACKEND_RIGETTI,        /**< Rigetti */
    QUANTUM_BACKEND_DWAVE,          /**< D-Wave (annealer) */
    QUANTUM_BACKEND_IONQ,           /**< IonQ (trapped ion) */
    QUANTUM_BACKEND_GOOGLE,         /**< Google Sycamore */
    QUANTUM_BACKEND_COUNT
} quantum_backend_t;

//=============================================================================
// Cognitive Module Flags (Bitmask)
//=============================================================================

/**
 * @brief Cognitive module enable flags
 *
 * WHAT: Bitmask for enabling/disabling cognitive modules
 * WHY:  Fine-grained control over which modules run at each tier
 * HOW:  Each bit represents a cognitive subsystem
 */
typedef enum {
    COGNITIVE_MODULE_NONE             = 0x00000000,  /**< No modules */

    // Core modules (highest priority)
    COGNITIVE_MODULE_ATTENTION        = 0x00000001,  /**< Attention system */
    COGNITIVE_MODULE_WORKING_MEMORY   = 0x00000002,  /**< Working memory */
    COGNITIVE_MODULE_SALIENCE         = 0x00000004,  /**< Salience detection */

    // Emotional modules
    COGNITIVE_MODULE_EMOTIONS         = 0x00000008,  /**< Emotional system */
    COGNITIVE_MODULE_EMOTIONAL_TAG    = 0x00000010,  /**< Emotional tagging */

    // Memory systems
    COGNITIVE_MODULE_SEMANTIC_MEMORY  = 0x00000020,  /**< Semantic memory */
    COGNITIVE_MODULE_EPISODIC_MEMORY  = 0x00000040,  /**< Episodic memory */
    COGNITIVE_MODULE_CONSOLIDATION    = 0x00000080,  /**< Memory consolidation */

    // Executive functions
    COGNITIVE_MODULE_EXECUTIVE        = 0x00000100,  /**< Executive control */
    COGNITIVE_MODULE_REASONING        = 0x00000200,  /**< Logical reasoning */
    COGNITIVE_MODULE_CURIOSITY        = 0x00000400,  /**< Curiosity system */

    // Meta-cognitive
    COGNITIVE_MODULE_META_LEARNING    = 0x00000800,  /**< Meta-learning */
    COGNITIVE_MODULE_INTROSPECTION    = 0x00001000,  /**< Introspection */
    COGNITIVE_MODULE_SELF_AWARENESS   = 0x00002000,  /**< Self-awareness */

    // Social cognition
    COGNITIVE_MODULE_THEORY_OF_MIND   = 0x00004000,  /**< Theory of mind */
    COGNITIVE_MODULE_MIRROR_NEURONS   = 0x00008000,  /**< Mirror neuron system */
    COGNITIVE_MODULE_EMPATHY          = 0x00010000,  /**< Empathetic response */

    // Advanced features
    COGNITIVE_MODULE_GLOBAL_WORKSPACE = 0x00020000,  /**< Global workspace */
    COGNITIVE_MODULE_PREDICTIVE       = 0x00040000,  /**< Predictive coding */
    COGNITIVE_MODULE_ETHICS           = 0x00080000,  /**< Ethical reasoning */

    // Perception (basic, usually enabled)
    COGNITIVE_MODULE_VISUAL_CORTEX    = 0x00100000,  /**< Visual processing */
    COGNITIVE_MODULE_AUDIO_CORTEX     = 0x00200000,  /**< Audio processing */

    // All modules
    COGNITIVE_MODULE_ALL              = 0xFFFFFFFF   /**< All modules */
} cognitive_module_flags_t;

//=============================================================================
// Visual Cortex Configuration
//=============================================================================

/**
 * @brief Visual cortex configuration per tier
 */
typedef struct {
    uint32_t max_input_width;      /**< Max input image width */
    uint32_t max_input_height;     /**< Max input image height */
    uint32_t num_filters_conv1;    /**< Conv layer 1 filters */
    uint32_t num_filters_conv2;    /**< Conv layer 2 filters */
    uint32_t max_feature_maps;     /**< Max feature maps */
    bool enable_pooling;           /**< Enable pooling layers */
    bool enable_attention;         /**< Enable visual attention */
} visual_cortex_tier_config_t;

//=============================================================================
// Audio Cortex Configuration
//=============================================================================

/**
 * @brief Audio cortex configuration per tier
 */
typedef struct {
    uint32_t max_sample_rate;      /**< Max sample rate (Hz) */
    uint32_t num_mel_filters;      /**< Mel-scale filter bank size */
    uint32_t num_mfcc;             /**< MFCC coefficients */
    uint32_t frame_size;           /**< Frame size (samples) */
    bool enable_attention;         /**< Enable auditory attention */
    bool enable_memory;            /**< Enable auditory memory */
} audio_cortex_tier_config_t;

//=============================================================================
// Platform Tier Configuration
//=============================================================================

/**
 * @brief Complete platform tier configuration
 *
 * WHAT: All configuration parameters for a specific tier
 * WHY:  Encapsulate tier-specific limits in one structure
 * HOW:  Pre-configured defaults for each tier, user can override
 *
 * THREAD SAFETY: Read-only after initialization (safe for concurrent reads)
 */
typedef struct {
    // Platform tier
    platform_tier_t tier;                    /**< Tier classification */

    // Core neural network constraints
    uint32_t max_neurons;                    /**< Max neurons in brain */
    uint32_t max_synapses_per_neuron;        /**< Max synapses per neuron */
    uint32_t initial_neurons;                /**< Initial neuron count */

    // Cognitive modules enabled
    uint32_t cognitive_modules_enabled;      /**< Bitmask of enabled modules */

    // Visual cortex configuration
    visual_cortex_tier_config_t visual;      /**< Visual cortex config */

    // Audio cortex configuration
    audio_cortex_tier_config_t audio;        /**< Audio cortex config */

    // Resource budgets
    uint32_t memory_budget_mb;               /**< Memory budget (MB) */
    uint64_t compute_budget_ops;             /**< Compute budget (ops/sec) */
    uint32_t max_threads;                    /**< Max parallel threads */

    // Feature flags
    bool enable_gpu;                         /**< Enable GPU acceleration */
    bool enable_bio_async;                   /**< Enable bio-async messaging */
    bool enable_plasticity;                  /**< Enable synaptic plasticity */
    bool enable_neuromodulation;             /**< Enable neuromodulators */
    bool enable_checkpointing;               /**< Enable state checkpointing */

    // Performance tuning
    uint32_t update_batch_size;              /**< Neurons per batch update */
    uint32_t spike_buffer_size;              /**< Spike event buffer size */
    float sampling_rate;                     /**< State sampling rate (0-1) */
} platform_tier_config_t;

//=============================================================================
// Neuromorphic Tier Configuration
//=============================================================================

/**
 * @brief Neuromorphic hardware-specific configuration
 *
 * WHAT: Configuration for spike-based neuromorphic processors
 * WHY:  These chips have unique constraints (spike routing, on-chip learning)
 * HOW:  Hardware-specific parameters for different neuromorphic platforms
 *
 * BIOLOGICAL RELEVANCE:
 * Neuromorphic chips are the closest to biological neural networks:
 * - Event-driven processing (like real neurons)
 * - On-chip plasticity (STDP in hardware)
 * - Asynchronous spike routing
 * - Ultra-low power (mW vs GW for GPUs)
 */
typedef struct {
    neuromorphic_hardware_t hardware;        /**< Hardware type */

    // Neuron configuration
    uint32_t max_neurons;                    /**< Max neurons on chip */
    uint32_t max_synapses_per_neuron;        /**< Fanout limit */
    uint32_t cores;                          /**< Number of neuromorphic cores */
    uint32_t neurons_per_core;               /**< Neurons per core */

    // Spike routing
    bool spike_native;                       /**< Native spike routing (no simulation) */
    uint32_t spike_buffer_depth;             /**< Spike queue depth */
    uint32_t max_spike_rate_hz;              /**< Max spike rate per neuron */
    uint32_t routing_delay_timesteps;        /**< Synaptic delay granularity */

    // On-chip plasticity
    bool stdp_hardware;                      /**< On-chip STDP support */
    bool homeostatic_hardware;               /**< On-chip homeostatic plasticity */
    float learning_rate_granularity;         /**< Min LR step (hardware limited) */

    // Power and timing
    uint32_t timestep_ns;                    /**< Simulation timestep (ns) */
    float power_budget_mw;                   /**< Power budget (milliwatts) */
    bool async_events;                       /**< Event-driven (vs clock-driven) */

    // Memory
    uint32_t on_chip_memory_kb;              /**< On-chip SRAM (KB) */
    uint32_t external_memory_mb;             /**< External memory (MB) */
    bool weight_sharing;                     /**< Support weight sharing */

    // Features enabled
    bool enable_axon_delay;                  /**< Programmable axon delays */
    bool enable_multi_compartment;           /**< Multi-compartment neurons */
    bool enable_dendritic_computation;       /**< Dendritic processing */
} neuromorphic_tier_config_t;

//=============================================================================
// Quantum Tier Configuration
//=============================================================================

/**
 * @brief Quantum-classical hybrid configuration
 *
 * WHAT: Configuration for quantum computing backends
 * WHY:  Quantum systems have unique capabilities and constraints
 * HOW:  Define qubit counts, coherence times, supported operations
 *
 * QUANTUM ADVANTAGE IN NEURAL NETWORKS:
 * - Superposition: Evaluate multiple states simultaneously
 * - Entanglement: Correlate distant decisions
 * - Interference: Amplify correct solutions
 * - Tunneling: Escape local minima in optimization
 *
 * NOTE: These are software abstractions - actual hardware requires
 *       specific SDK integration (Qiskit, Cirq, Ocean, etc.)
 */
typedef struct {
    quantum_backend_t backend;               /**< Quantum backend type */

    // Qubit configuration
    uint32_t num_qubits;                     /**< Number of qubits available */
    uint32_t num_logical_qubits;             /**< Logical qubits (after error correction) */
    float connectivity;                      /**< Qubit connectivity [0-1] */

    // Coherence and errors
    float t1_coherence_us;                   /**< T1 relaxation time (microseconds) */
    float t2_coherence_us;                   /**< T2 dephasing time (microseconds) */
    float single_qubit_error;                /**< Single-qubit gate error rate */
    float two_qubit_error;                   /**< Two-qubit gate error rate */
    float readout_error;                     /**< Measurement error rate */

    // Gate set
    bool native_gates_clifford;              /**< Clifford gates native */
    bool native_gates_t;                     /**< T gate native */
    bool native_gates_rx_ry;                 /**< Arbitrary rotation gates */
    uint32_t max_circuit_depth;              /**< Max circuit depth */

    // Capabilities
    bool quantum_consensus;                  /**< Use quantum for swarm consensus */
    bool quantum_optimization;               /**< Use quantum for optimization (QAOA) */
    bool quantum_sampling;                   /**< Use quantum for sampling (QML) */
    bool superposition_states;               /**< Parallel state evaluation */

    // Hybrid configuration
    uint32_t classical_cores;                /**< Classical CPU cores for hybrid */
    uint32_t classical_memory_gb;            /**< Classical memory for hybrid */
    float quantum_classical_ratio;           /**< Fraction of work on quantum */

    // Execution
    uint32_t max_shots;                      /**< Max measurement shots */
    uint32_t queue_depth;                    /**< Job queue depth */
    bool real_hardware;                      /**< True if real quantum hardware */
} quantum_tier_config_t;

//=============================================================================
// Basic Tier Configuration (Ultra-Constrained)
//=============================================================================

/**
 * @brief Configuration for ultra-constrained MCU platforms
 *
 * WHAT: Minimal configuration for Arduino/ESP32/STM32 class devices
 * WHY:  These devices have severe resource constraints
 * HOW:  Disable all optional features, minimal memory footprint
 */
typedef struct {
    // Core constraints
    uint32_t max_neurons;                    /**< Very limited (100-1000) */
    uint32_t max_synapses;                   /**< Total synapses (not per neuron) */
    uint32_t memory_kb;                      /**< Total memory budget (KB) */

    // Fixed-point arithmetic (no FPU on some MCUs)
    bool use_fixed_point;                    /**< Use fixed-point instead of float */
    uint8_t fixed_point_bits;                /**< Q-format bits (e.g., Q15) */

    // Minimal features
    bool spike_only;                         /**< Spike-based only (no rate coding) */
    bool reactive_only;                      /**< Pure reactive (no learning) */
    bool single_layer;                       /**< Single layer only */

    // Power management
    float power_budget_mw;                   /**< Power budget (milliwatts) */
    uint32_t sleep_threshold_ms;             /**< Sleep after inactivity (ms) */
    bool deep_sleep_enabled;                 /**< Deep sleep mode */
} basic_tier_config_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Detect platform tier from system resources
 *
 * WHAT: Automatically classify platform into appropriate tier
 * WHY:  Enable automatic configuration without user intervention
 * HOW:  Query system resources (RAM, cores) and apply thresholds
 *
 * DETECTION ALGORITHM:
 * 1. Query system_resources_query() for RAM and CPU info
 * 2. Apply tier thresholds:
 *    - FULL: ≥8GB RAM AND ≥8 cores
 *    - MEDIUM: ≥2GB RAM AND ≥4 cores
 *    - CONSTRAINED: ≥256MB RAM AND ≥2 cores
 *    - MINIMAL: Otherwise
 * 3. Return detected tier
 *
 * THREAD SAFETY: Thread-safe (calls system_resources_query internally)
 *
 * @return Detected platform tier
 *
 * EXAMPLE:
 * @code
 * platform_tier_t tier = platform_tier_detect();
 * platform_tier_config_t config = platform_tier_get_config(tier);
 * brain_t* brain = brain_create(config.max_neurons, config.initial_neurons);
 * @endcode
 */
platform_tier_t platform_tier_detect(void);

/**
 * @brief Get pre-configured settings for specific tier
 *
 * WHAT: Retrieve default configuration for a given tier
 * WHY:  Provide sensible defaults tuned for each hardware class
 * HOW:  Return pre-computed configuration structure
 *
 * THREAD SAFETY: Thread-safe (returns const data)
 *
 * @param tier Platform tier
 * @return Configuration for that tier (safe to copy)
 *
 * EXAMPLE:
 * @code
 * platform_tier_config_t config = platform_tier_get_config(PLATFORM_TIER_MEDIUM);
 * printf("Max neurons: %u\n", config.max_neurons);
 * printf("Visual resolution: %ux%u\n", config.visual.max_input_width,
 *        config.visual.max_input_height);
 * @endcode
 */
platform_tier_config_t platform_tier_get_config(platform_tier_t tier);

/**
 * @brief Get human-readable tier name
 *
 * WHAT: Convert tier enum to descriptive string
 * WHY:  Useful for logging, debugging, user messages
 * HOW:  Simple lookup table
 *
 * THREAD SAFETY: Thread-safe (returns const string)
 *
 * @param tier Platform tier
 * @return Tier name string (e.g., "FULL", "MEDIUM", "CONSTRAINED", "MINIMAL")
 *
 * EXAMPLE:
 * @code
 * platform_tier_t tier = platform_tier_detect();
 * printf("Running on %s platform\n", platform_tier_get_name(tier));
 * // Output: "Running on MEDIUM platform"
 * @endcode
 */
const char* platform_tier_get_name(platform_tier_t tier);

/**
 * @brief Check if cognitive module is enabled at tier
 *
 * WHAT: Test if specific module is available at given tier
 * WHY:  Allow runtime checks before using optional modules
 * HOW:  Bitwise AND with tier's enabled module bitmask
 *
 * THREAD SAFETY: Thread-safe
 *
 * @param tier Platform tier
 * @param module Module flag to check (cognitive_module_flags_t)
 * @return true if module is enabled at this tier, false otherwise
 *
 * EXAMPLE:
 * @code
 * platform_tier_t tier = platform_tier_detect();
 * if (platform_tier_can_enable_module(tier, COGNITIVE_MODULE_CURIOSITY)) {
 *     // Safe to use curiosity system
 *     curiosity_system_init();
 * } else {
 *     printf("Curiosity disabled on this platform\n");
 * }
 * @endcode
 */
bool platform_tier_can_enable_module(platform_tier_t tier,
                                      cognitive_module_flags_t module);

/**
 * @brief Get recommended neuron count for tier
 *
 * WHAT: Calculate appropriate neuron count based on tier and available resources
 * WHY:  Balance functionality with memory constraints
 * HOW:  Use tier defaults, constrained by actual available RAM
 *
 * THREAD SAFETY: Thread-safe
 *
 * @param tier Platform tier
 * @param resources Current system resources (from system_resources_query)
 * @return Recommended neuron count
 *
 * EXAMPLE:
 * @code
 * system_resources_t resources;
 * system_resources_query(&resources);
 * platform_tier_t tier = platform_tier_detect();
 * uint32_t neurons = platform_tier_recommend_neuron_count(tier, &resources);
 * @endcode
 */
uint32_t platform_tier_recommend_neuron_count(platform_tier_t tier,
                                               const system_resources_t* resources);

/**
 * @brief Validate custom config against tier constraints
 *
 * WHAT: Check if user-provided config exceeds tier limits
 * WHY:  Prevent OOM/performance issues from over-ambitious configs
 * HOW:  Compare user config against tier maximums
 *
 * THREAD SAFETY: Thread-safe (read-only operations)
 *
 * @param tier Platform tier
 * @param config User configuration to validate
 * @param error_msg Buffer for error message (can be NULL)
 * @param error_msg_len Size of error message buffer
 * @return true if config is valid for tier, false if it exceeds limits
 *
 * EXAMPLE:
 * @code
 * platform_tier_config_t config = platform_tier_get_config(tier);
 * config.max_neurons = 10000000;  // User wants 10M neurons
 *
 * char error[256];
 * if (!platform_tier_validate_config(tier, &config, error, sizeof(error))) {
 *     printf("Invalid config: %s\n", error);
 * }
 * @endcode
 */
bool platform_tier_validate_config(platform_tier_t tier,
                                    const platform_tier_config_t* config,
                                    char* error_msg,
                                    size_t error_msg_len);

//=============================================================================
// Specialized Tier API Functions
//=============================================================================

/**
 * @brief Get default configuration for BASIC tier (ultra-constrained MCU)
 *
 * WHAT: Returns configuration for Arduino/ESP32 class devices
 * WHY:  These platforms need specific handling due to extreme constraints
 * HOW:  Pre-configured defaults for typical MCU capabilities
 *
 * @return Basic tier configuration with sensible defaults
 *
 * EXAMPLE:
 * @code
 * basic_tier_config_t config = platform_tier_get_basic_config();
 * // max_neurons: 500, memory_kb: 32, use_fixed_point: true
 * @endcode
 */
basic_tier_config_t platform_tier_get_basic_config(void);

/**
 * @brief Get default configuration for neuromorphic hardware
 *
 * WHAT: Returns configuration for specific neuromorphic chip
 * WHY:  Each chip has unique constraints and capabilities
 * HOW:  Pre-configured defaults based on known hardware specs
 *
 * @param hardware Neuromorphic hardware type
 * @return Neuromorphic configuration for that hardware
 *
 * EXAMPLE:
 * @code
 * neuromorphic_tier_config_t config = platform_tier_get_neuromorphic_config(NEUROMORPHIC_HW_LOIHI);
 * // max_neurons: 1M, spike_native: true, stdp_hardware: true
 * @endcode
 */
neuromorphic_tier_config_t platform_tier_get_neuromorphic_config(neuromorphic_hardware_t hardware);

/**
 * @brief Get default configuration for quantum backend
 *
 * WHAT: Returns configuration for specific quantum platform
 * WHY:  Each backend has different qubit counts and error rates
 * HOW:  Pre-configured defaults based on known hardware/simulator specs
 *
 * @param backend Quantum backend type
 * @return Quantum configuration for that backend
 *
 * EXAMPLE:
 * @code
 * quantum_tier_config_t config = platform_tier_get_quantum_config(QUANTUM_BACKEND_SIMULATOR);
 * // num_qubits: 32, real_hardware: false, quantum_consensus: true
 * @endcode
 */
quantum_tier_config_t platform_tier_get_quantum_config(quantum_backend_t backend);

/**
 * @brief Check if tier is a specialized (non-standard) tier
 *
 * WHAT: Test if tier is NEUROMORPHIC or QUANTUM (vs standard compute tiers)
 * WHY:  Specialized tiers need different handling
 * HOW:  Simple range check
 *
 * @param tier Platform tier to check
 * @return true if NEUROMORPHIC or QUANTUM, false for standard tiers
 */
bool platform_tier_is_specialized(platform_tier_t tier);

/**
 * @brief Get human-readable name for neuromorphic hardware
 *
 * @param hardware Hardware type
 * @return Hardware name string (e.g., "Loihi", "SpiNNaker")
 */
const char* platform_tier_neuromorphic_name(neuromorphic_hardware_t hardware);

/**
 * @brief Get human-readable name for quantum backend
 *
 * @param backend Backend type
 * @return Backend name string (e.g., "IBM Quantum", "D-Wave")
 */
const char* platform_tier_quantum_name(quantum_backend_t backend);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PLATFORM_TIER_H
