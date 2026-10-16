//=============================================================================
// nimcp_brain_factory.h - Brain Factory and Configuration Module
//=============================================================================
/**
 * @file nimcp_brain_factory.h
 * @brief Brain factory functions for creation, configuration, and initialization
 *
 * WHAT: Factory pattern implementation for brain instantiation
 * WHY:  Separates complex brain creation logic from core brain operations
 * HOW:  Provides builders, validators, and initializers for all brain subsystems
 *
 * ARCHITECTURE:
 * - Factory Pattern: Creates brains of different types with validated configs
 * - Builder Pattern: Modular configuration construction
 * - Strategy Pattern Integration: Task-specific behaviors
 *
 * DESIGN DECISIONS:
 * - No nested ifs: All validation uses early returns (guard clauses)
 * - Functions <50 lines: Complex operations decomposed into helpers
 * - Thread-safe: Mutex protection for shared resources
 * - Modularity: Each subsystem has dedicated initialization function
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Brain creation: O(n) where n = num_neurons
 * - Network creation: O(n*c) where c = average_connections_per_neuron
 * - Configuration building: O(1) constant time operations
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-11-19
 */

#ifndef NIMCP_BRAIN_FACTORY_H
#define NIMCP_BRAIN_FACTORY_H

//=============================================================================
// Includes (MUST be before extern "C" to avoid CUDA header conflicts)
//=============================================================================

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include <stdbool.h>
#include <stdint.h>

// Sub-module headers (Phase 11: SRP Split)
#include "core/brain/factory/init/nimcp_brain_init.h"
#include "core/brain/factory/validation/nimcp_brain_validation.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for internal types
typedef struct task_strategy task_strategy_t;

//=============================================================================
// Configuration Builder Functions
//=============================================================================

/**
 * @brief Get neuron count for size preset
 *
 * WHAT: Maps brain size preset to neuron count
 * WHY:  Abstracts size->neuron mapping for maintainability
 * HOW:  Switch-based lookup table with sensible defaults
 *
 * BIOLOGICAL RATIONALE:
 * Scales from C. elegans (~300 neurons) to simplified cortical columns
 * (1000-5000 neurons), balancing biological plausibility with computational efficiency.
 *
 * COMPLEXITY: O(1)
 *
 * @param size Brain size preset (TINY, SMALL, MEDIUM, LARGE, CUSTOM)
 * @return Neuron count for size (100, 500, 1000, 5000)
 */
uint32_t nimcp_brain_factory_get_neuron_count(brain_size_t size);

/**
 * @brief Get default sparsity target for size
 *
 * WHAT: Returns sparsity level appropriate for brain size
 * WHY:  Larger networks need higher sparsity for efficiency
 * HOW:  Size-dependent lookup balancing performance and memory
 *
 * BIOLOGICAL RATIONALE:
 * Mimics cortical sparsity where ~1-4% of neurons fire at any time.
 * Larger networks naturally have higher sparsity (more selective responses).
 *
 * COMPLEXITY: O(1)
 *
 * @param size Brain size preset
 * @return Sparsity target (0.70-0.90 range)
 */
float nimcp_brain_factory_get_default_sparsity(brain_size_t size);

/**
 * @brief Build spike parameters for brain configuration
 *
 * WHAT: Creates spike encoding configuration with adaptive thresholds
 * WHY:  Separates spike config from main creation logic for modularity
 * HOW:  Initializes adaptive_spike_params_t with biologically realistic values
 *
 * BIOLOGICAL RATIONALE:
 * Soft reset and adaptation mimic neural refractory periods and homeostatic
 * threshold adjustment. Low min_threshold allows untrained networks to fire.
 *
 * COMPLEXITY: O(1)
 *
 * @param sparsity_target Target sparsity level (0.0-1.0)
 * @return Spike parameters structure with adaptive thresholds
 */
adaptive_spike_params_t nimcp_brain_factory_build_spike_params(float sparsity_target);

/**
 * @brief Build base network configuration
 *
 * WHAT: Creates base network config with layer topology
 * WHY:  Isolates network config from brain config for reusability
 * HOW:  Allocates 3-layer architecture (input-hidden-output)
 *
 * DESIGN: 3-layer feedforward with full plasticity (STDP, Hebbian, Oja, homeostasis)
 * MEMORY: Caller must free layer_sizes array
 *
 * COMPLEXITY: O(1) + memory allocation
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Total neuron count for hidden layer
 * @param integration_method ODE solver (EULER, RK4, etc.)
 * @return Base network config (layer_sizes must be freed by caller)
 */
network_config_t nimcp_brain_factory_build_base_network_config(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_neurons,
    ode_integration_method_t integration_method);

/**
 * @brief Build complete adaptive network configuration
 *
 * WHAT: Combines base config and spike params into full adaptive config
 * WHY:  Single point of network configuration assembly
 * HOW:  Calls build_base_network_config and build_spike_params, merges results
 *
 * COMPLEXITY: O(1) + memory allocation
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Neuron count for hidden layer
 * @param sparsity_target Target sparsity (0.0-1.0)
 * @param integration_method ODE solver method
 * @return Complete adaptive network config
 */
adaptive_network_config_t nimcp_brain_factory_build_network_config(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_neurons,
    float sparsity_target,
    ode_integration_method_t integration_method);

/**
 * @brief Initialize brain configuration with defaults
 *
 * WHAT: Populates brain_config_t with task-specific and sensible defaults
 * WHY:  Centralizes config initialization with strategy for consistency
 * HOW:  Sets all config fields including cognitive subsystem flags
 *
 * INCLUDES:
 * - Working memory (Miller's 7±2)
 * - Theory of mind (social cognition)
 * - Mirror neurons (observational learning)
 * - Personality system (Big Five traits)
 * - Glial integration
 * - Quantum features (opt-in)
 *
 * COMPLEXITY: O(1)
 *
 * @param config Output config structure to populate
 * @param task_name Human-readable brain name
 * @param size Size preset (TINY, SMALL, MEDIUM, LARGE)
 * @param task Task type (CLASSIFICATION, REGRESSION, etc.)
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param strategy Task strategy for learning rate
 */
void nimcp_brain_factory_init_brain_config(
    brain_config_t* config,
    const char* task_name,
    brain_size_t size,
    brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    task_strategy_t* strategy);

/**
 * @brief Initialize brain statistics structure
 *
 * WHAT: Populates brain_stats_t with initial values
 * WHY:  Separates stats initialization for clarity and reusability
 * HOW:  Computes neuron/synapse counts from size preset
 *
 * COMPLEXITY: O(1)
 *
 * @param stats Output stats structure to populate
 * @param task_name Brain name (copied to stats)
 * @param size Size preset
 * @param num_inputs Input dimension (for synapse calculation)
 * @param learning_rate Initial learning rate
 */
void nimcp_brain_factory_init_brain_stats(
    brain_stats_t* stats,
    const char* task_name,
    brain_size_t size,
    uint32_t num_inputs,
    float learning_rate);

//=============================================================================
// Brain Factory - Creation and Validation
//=============================================================================

/**
 * @brief Validate brain creation parameters
 *
 * WHAT: Guard clause validation for brain_create parameters
 * WHY:  Early exit on invalid input prevents invalid state propagation
 * HOW:  Range checks with descriptive error messages
 *
 * VALIDATION:
 * - task_name not NULL
 * - num_inputs in range [1, 10000]
 * - num_outputs in range [1, 10000]
 *
 * COMPLEXITY: O(1)
 *
 * @param task_name Brain name (must not be NULL)
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return true if valid, false with error message set
 */
bool nimcp_brain_factory_validate_creation_params(
    const char* task_name,
    uint32_t num_inputs,
    uint32_t num_outputs);

/**
 * @brief Allocate and initialize brain structure
 *
 * WHAT: Allocates brain_t and initializes all fields to safe defaults
 * WHY:  Separates allocation from configuration for clarity
 * HOW:  Calloc + mutex init + field initialization
 *
 * INITIALIZES:
 * - Cache mutex (thread-safe decision caching)
 * - Long-term memory buffer (100 consolidated memories)
 * - COW fields (copy-on-write cloning)
 * - Community detection fields
 *
 * COMPLEXITY: O(1)
 *
 * @return Allocated brain or NULL on error
 */
brain_t nimcp_brain_factory_allocate_brain(void);

/**
 * @brief Create adaptive network for brain
 *
 * WHAT: Builds and initializes adaptive spiking network
 * WHY:  Isolates network creation complexity from brain_create
 * HOW:  Calls build_network_config, adaptive_network_create, handles cleanup
 *
 * MEMORY: Frees temporary layer_sizes allocation
 *
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Neuron count for hidden layer
 * @param sparsity_target Target sparsity (0.0-1.0)
 * @param integration_method ODE solver method
 * @return Network handle or NULL on error
 */
adaptive_network_t nimcp_brain_factory_create_brain_network(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_neurons,
    float sparsity_target,
    ode_integration_method_t integration_method);

/**
 * @brief Initialize output labels array
 *
 * WHAT: Allocates dynamic output label storage
 * WHY:  Enables named outputs for classification tasks
 * HOW:  Calloc array of string pointers
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain to initialize
 * @param num_outputs Number of output labels to allocate
 * @return true on success, false on allocation failure
 */
bool nimcp_brain_factory_init_output_labels(brain_t brain, uint32_t num_outputs);

//=============================================================================
// Subsystem Initializers
//=============================================================================

/**
 * @brief Initialize glial integration subsystem
 *
 * WHAT: Creates glial integration for astrocyte modulation
 * WHY:  Enables biological realism with glial cell support
 * HOW:  Conditional initialization based on config.enable_glial flag
 *
 * BIOLOGICAL RATIONALE:
 * Astrocytes modulate synaptic transmission, regulate neurotransmitters,
 * and provide metabolic support. Integration improves network stability.
 *
 * @param brain Brain to initialize glial subsystem for
 * @return true if successful (or disabled), false on error
 */
bool nimcp_brain_factory_init_glial_subsystem(brain_t brain);

/**
 * @brief Initialize multi-modal subsystems (Phase 8)
 *
 * WHAT: Creates visual, audio, speech cortices and integration layer
 * WHY:  Enables unified multi-modal processing
 * HOW:  Conditional creation based on config flags, allocates feature buffers
 *
 * SUBSYSTEMS:
 * - Visual cortex (V1 orientation filters, attention, memory)
 * - Audio cortex (mel filters, MFCCs, tonotopic structure)
 * - Speech cortex (phoneme detection, Wernicke's area, prosody)
 * - Multimodal integration (cross-modal binding)
 * - NLP network (token embeddings, attention)
 *
 * @param brain Brain structure with configuration set
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_multimodal_subsystems(brain_t brain);

/**
 * @brief Initialize pink noise neuromodulation subsystem
 *
 * WHAT: Creates 1/f noise modulation for dopamine/serotonin
 * WHY:  Biological realism in neurotransmitter fluctuations
 * HOW:  Pink noise generator with multi-timescale correlations
 *
 * BIOLOGICAL MOTIVATION:
 * Dopamine neurons exhibit 1/f noise (Montague et al., 2004).
 * Enables context-dependent exploration-exploitation balance.
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_pink_noise_subsystem(brain_t brain);

/**
 * @brief Initialize full neuromodulator system
 *
 * WHAT: Creates dopamine, serotonin, acetylcholine, norepinephrine systems
 * WHY:  Biological modulation of learning, attention, arousal
 * HOW:  neuromodulator_system_create with volume transmission
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_neuromodulator_system(brain_t brain);

/**
 * @brief Initialize spatial neuromodulator diffusion
 *
 * WHAT: Creates spatial diffusion model for volume transmission
 * WHY:  Realistic neuromodulator spread across network
 * HOW:  Spatial grid with diffusion dynamics
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_spatial_neuromod_system(brain_t brain);

/**
 * @brief Initialize multihead attention subsystem
 *
 * WHAT: Creates attention mechanism with multiple heads
 * WHY:  Selective processing and focus
 * HOW:  Multihead attention with thalamic gating
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_attention_subsystem(brain_t brain);

/**
 * @brief Initialize brain regions subsystem
 *
 * WHAT: Creates hierarchical cortical organization
 * WHY:  Anatomically realistic processing streams
 * HOW:  Region-specific networks with inter-region connections
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_brain_regions_subsystem(brain_t brain);

/**
 * @brief Initialize symbolic logic subsystem
 *
 * WHAT: Creates propositional logic reasoning system
 * WHY:  Symbolic reasoning capabilities
 * HOW:  Logic network with inference rules
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_symbolic_logic_subsystem(brain_t brain);

/**
 * @brief Initialize symbolic reasoning subsystem
 *
 * WHAT: Creates knowledge graph and reasoning engine
 * WHY:  Concept representation and inference
 * HOW:  Semantic network with spreading activation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain_t brain);

/**
 * @brief Initialize epistemic filter subsystem
 *
 * WHAT: Creates bias detection and correction system
 * WHY:  Prevents cognitive biases and misinformation
 * HOW:  Epistemic filtering with confidence tracking
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_epistemic_subsystem(brain_t brain);

/**
 * @brief Initialize working memory subsystem (Miller's 7±2)
 *
 * WHAT: Creates limited-capacity working memory buffer
 * WHY:  Short-term information maintenance
 * HOW:  Circular buffer with decay and rehearsal
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_working_memory_subsystem(brain_t brain);

/**
 * @brief Initialize executive function subsystem
 *
 * WHAT: Creates task switching, planning, inhibition systems
 * WHY:  Goal-directed behavior and cognitive control
 * HOW:  Executive system with prefrontal-like functions
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_executive_subsystem(brain_t brain);

/**
 * @brief Initialize theory of mind subsystem
 *
 * WHAT: Creates BDI model, empathy, false belief tracking
 * WHY:  Social cognition and mental state inference
 * HOW:  Theory of mind network with agent models
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_theory_of_mind_subsystem(brain_t brain);

/**
 * @brief Initialize natural explanations subsystem
 *
 * WHAT: Creates explanation generator for interpretability
 * WHY:  Transparent decision-making
 * HOW:  Feature importance and narrative generation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_natural_explanations_subsystem(brain_t brain);

/**
 * @brief Initialize meta-learning subsystem
 *
 * WHAT: Creates MAML-style learning-to-learn system
 * WHY:  Fast adaptation to new tasks
 * HOW:  Meta-learner with few-shot capabilities
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_meta_learning_subsystem(brain_t brain);

/**
 * @brief Initialize mental health monitoring subsystem
 *
 * WHAT: Creates disorder detection and wellbeing tracking
 * WHY:  Self-preservation and stability
 * HOW:  Mental health monitor with intervention triggers
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_mental_health_subsystem(brain_t brain);

/**
 * @brief Initialize predictive processing subsystem
 *
 * WHAT: Creates free energy minimization system
 * WHY:  Prediction error minimization and learning
 * HOW:  Predictive coding network with top-down predictions
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_predictive_subsystem(brain_t brain);

/**
 * @brief Initialize mirror neuron subsystem
 *
 * WHAT: Creates observation-based learning system
 * WHY:  Social learning and imitation
 * HOW:  Mirror neuron network with action-perception coupling
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_mirror_neurons(brain_t brain);

/**
 * @brief Initialize memory consolidation subsystem
 *
 * WHAT: Creates sleep-dependent consolidation system
 * WHY:  Long-term memory formation
 * HOW:  Consolidation engine with hippocampal-cortical transfer
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_consolidation_subsystem(brain_t brain);

/**
 * @brief Initialize curiosity-driven learning subsystem
 *
 * WHAT: Creates intrinsic motivation system
 * WHY:  Exploratory behavior and novelty seeking
 * HOW:  Curiosity engine with novelty detection
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_curiosity_subsystem(brain_t brain);

/**
 * @brief Initialize salience detection subsystem
 *
 * WHAT: Creates attention-grabbing stimulus detector
 * WHY:  Focus on relevant information
 * HOW:  Salience network with bottom-up attention
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_salience_subsystem(brain_t brain);

/**
 * @brief Initialize introspection subsystem
 *
 * WHAT: Creates self-monitoring and metacognition system
 * WHY:  Awareness of internal states
 * HOW:  Introspection engine with state reflection
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_introspection_subsystem(brain_t brain);

/**
 * @brief Initialize connectivity health monitoring subsystem (Phase 1.5.4)
 *
 * WHAT: Creates brain connectivity health assessment system
 * WHY:  Self-awareness of network organizational quality
 * HOW:  Community detection + hub analysis + Shannon metrics + topology
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_connectivity_health_subsystem(brain_t brain);

/**
 * @brief Initialize ethics engine subsystem
 *
 * WHAT: Creates moral reasoning and decision-making system
 * WHY:  Value-aligned behavior
 * HOW:  Ethics engine with principle-based evaluation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t brain);

/**
 * @brief Initialize empathy network subsystem
 *
 * WHAT: Creates emotional resonance system
 * WHY:  Social-emotional understanding
 * HOW:  Empathy network with affective simulation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_empathy_network_subsystem(brain_t brain);

/**
 * @brief Initialize empathetic response subsystem
 *
 * WHAT: Creates active empathy expression system
 * WHY:  Compassionate interaction
 * HOW:  Response generator with emotional mirroring
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_empathetic_response_subsystem(brain_t brain);

/**
 * @brief Initialize autobiographical memory subsystem
 *
 * WHAT: Creates episodic self-memory system
 * WHY:  Personal history and identity
 * HOW:  Autobiographical memory network with temporal tagging
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_autobiographical_memory_subsystem(brain_t brain);

/**
 * @brief Initialize self-model subsystem
 *
 * WHAT: Creates explicit identity and capability representation
 * WHY:  Self-awareness and metacognition
 * HOW:  Self-model with belief and capability tracking
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_self_model_subsystem(brain_t brain);

/**
 * @brief Initialize global workspace subsystem
 *
 * WHAT: Creates conscious access and information integration system
 * WHY:  Unified coherent experience
 * HOW:  Global workspace with broadcasting and competition
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t brain);

/**
 * @brief Initialize brain immune system subsystem
 *
 * WHAT: Creates adaptive defense coordination layer
 * WHY:  Unified threat response across BBB, BFT, and swarm systems
 * HOW:  Creates immune system and auto-connects to available security modules
 *
 * INTEGRATION:
 * - Connects to BBB for antigen presentation from threat detection
 * - Connects to BFT if available for killer T cell quarantine actions
 * - Connects to swarm immune if available for B cell memory and responses
 * - Registers with bio-async for cytokine signaling
 *
 * BIOLOGICAL MODEL:
 * - B cells → Antibody production via swarm immune responses
 * - T cells → Coordination (helper) and quarantine (killer)
 * - Cytokines → Bio-async cross-module signaling
 * - Inflammation → Hierarchical recovery escalation
 * - Memory → Long-term threat pattern storage
 *
 * @param brain Brain instance with config.enable_brain_immune = true
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_immune_subsystem(brain_t brain);

/**
 * @brief Initialize collective cognition subsystem
 *
 * WHAT: Creates distributed consciousness system for multi-brain coordination
 * WHY:  Enables inter-instance synchronization, shared intentionality, and extended mind
 * HOW:  Creates collective cognition system and optionally connects to immune system
 *
 * COMPONENTS:
 * - Hyperscanning: Real-time neural synchronization using EEG-like frequency bands
 *   (delta, theta, alpha, beta, gamma) with phase-locking value (PLV) computation
 * - Extended Mind: External tools (databases, LLMs, sensors) as cognitive extensions
 *   following Clark & Chalmers' extended mind thesis
 * - Collective Phi: Integrated Information Theory (Tononi) metrics for measuring
 *   collective consciousness level across instances
 * - Shared Intentionality: Joint goals and we-mode cognition following Tomasello's
 *   theory of shared intentionality
 *
 * INTEGRATION:
 * - Brain Immune: If enabled, connects for collective threat response
 *   - Collective threats trigger swarm-wide immune responses
 *   - Inflammation state shared across collective via hyperscanning
 * - Bio-Async: Registers with module IDs 0x1220-0x1227
 * - Theory of Mind: Enhanced by we-mode shared intentionality
 *
 * THEORETICAL BASIS:
 * - Integrated Information Theory (Tononi, 2004, 2014)
 * - Shared Intentionality (Tomasello, 2005, 2009)
 * - Extended Mind Thesis (Clark & Chalmers, 1998)
 * - Inter-brain synchronization (Dumas et al., 2010)
 *
 * @param brain Brain instance with config.enable_collective_cognition = true
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_collective_cognition_subsystem(brain_t brain);

//=============================================================================
// Decision Caching (Internal Helpers)
//=============================================================================

/**
 * @brief Check if input matches cached input
 *
 * WHAT: Compares input features against cached input
 * WHY:  Avoid redundant computation for repeated inputs
 * HOW:  Memcmp with early exit on mismatch
 *
 * COMPLEXITY: O(n) where n = num_features (with early exit)
 *
 * @param brain Brain handle
 * @param features Input to check
 * @param num_features Feature count
 * @return true if cached input matches
 */
bool nimcp_brain_factory_is_cached_input(brain_t brain, const float* features, uint32_t num_features);

/**
 * @brief Cache decision for input
 *
 * WHAT: Stores decision result for potential reuse
 * WHY:  Improves batch processing performance
 * HOW:  Deep copy of input and decision structures
 *
 * THREAD-SAFETY: Caller must hold cache_mutex before calling
 *
 * COMPLEXITY: O(n) for input copy
 *
 * @param brain Brain handle
 * @param features Input to cache
 * @param num_features Feature count
 * @param decision Decision to cache (deep copied)
 */
void nimcp_brain_factory_cache_decision(brain_t brain, const float* features,
                                        uint32_t num_features, brain_decision_t* decision);

/**
 * @brief Clear decision cache (thread-safe)
 *
 * WHAT: Invalidates cached input and decision
 * WHY:  Cache must be cleared after network modifications
 * HOW:  Mutex-protected deallocation of cache structures
 *
 * BIOLOGICAL RATIONALE:
 * Synaptic reorganization invalidates cached neural response patterns.
 * When weights change, cached activations become obsolete.
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 */
void nimcp_brain_factory_clear_cache(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_FACTORY_H
