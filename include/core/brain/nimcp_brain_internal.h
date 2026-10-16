//=============================================================================
// nimcp_brain_internal.h - Internal Brain Structure Definition
//=============================================================================
/**
 * @file nimcp_brain_internal.h
 * @brief Internal header exposing brain_struct and task_strategy_t for brain modules
 *
 * WHAT: Internal definitions for brain implementation modules
 * WHY:  Extracted brain modules (factory, learning, inference, persistence,
 *       distributed, strategy) need access to brain internal structures
 * HOW:  Exposes brain_struct and task_strategy_t definitions extracted from nimcp_brain.c
 *
 * WARNING: THIS IS FOR INTERNAL USE ONLY
 * ========================================
 * DO NOT include this header in public-facing code - use nimcp_brain.h instead.
 * This header exposes internal implementation details that are subject to change.
 * Only brain implementation modules should include this file.
 *
 * VISIBILITY:
 * - brain_struct: Complete internal structure of the brain
 * - task_strategy_t: Strategy pattern interface for task-specific behaviors
 *
 * DESIGN:
 * - Allows modular decomposition of brain implementation
 * - Maintains encapsulation at module boundaries
 * - Supports clean separation of concerns
 */

#ifndef NIMCP_BRAIN_INTERNAL_H
#define NIMCP_BRAIN_INTERNAL_H

//=============================================================================
// Required Headers for Type Definitions
//=============================================================================

#include "core/brain/nimcp_brain.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/containers/nimcp_hash_table.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "security/nimcp_security.h"
#include "security/nimcp_security_integration.h"  // Phase SC-4: Universal Security Integration
#include "security/nimcp_blood_brain_barrier.h"   // Phase IS-1: BBB Perimeter Defense
#include "core/brain/learning/nimcp_brain_experience.h"  // Unified Experience API
#include "generation/nimcp_tokenizer.h"                  // Persistent tokenizer

/* Biology cluster — forward decls only. Each biology header typedefs its own
 * `nimcp_brain_t` which conflicts with the public nimcp.h typedef, so we
 * avoid full-including those headers here. Users of the biology modules
 * should #include the module header directly in their .c files. */
typedef struct nimcp_epigenetics_struct* nimcp_epigenetics_t;
typedef struct nimcp_neurogenesis_struct* nimcp_neurogenesis_t;
typedef struct nimcp_nvc_system_s nimcp_nvc_system_t;

/* Predictive-immune coupling — forward decl only. The full header
 * (cognitive/nimcp_predictive_immune.h) pulls in predictive + immune +
 * bio_messages headers, some of which redefine nimcp_brain_t. Consumers
 * include the module header directly from their .c files. The opaque
 * `predictive_immune_system` struct tag matches the typedef in
 * nimcp_predictive_immune.h (no `_s` suffix). */
typedef struct predictive_immune_system predictive_immune_system_t;

/* Cochlea + 15 consumer bridges — forward decls only. The bridge headers
 * typedef their own opaque structs locally; to avoid pulling the whole
 * perception subtree (cochlea.h → basilar_membrane.h → hair_cells.h → ...)
 * into every TU that includes brain_internal.h, we forward-declare the
 * struct tags here. Consumers (#include the bridge header in their .c). */
struct cochlea;
struct cochlea_audio_cortex_bridge;
struct cochlea_bio_async_bridge;
struct cochlea_broca_bridge;
struct cochlea_collective_bridge;
struct cochlea_cortical_deep_bridge;
struct cochlea_fep_bridge;
struct cochlea_immune_bridge;
struct cochlea_kg_bridge;
struct cochlea_medulla_bridge;
struct cochlea_occipital_bridge;
struct cochlea_rcog_bridge;
struct cochlea_sleep_bridge;
struct cochlea_substrate_bridge;
struct cochlea_thalamic_bridge;
struct cochlea_verification_bridge;

/* Physics bridge forward decls (Wave 8C, 2026-04-24). Like the cochlea
 * bridges above, these are opaque pointer types declared here to avoid
 * pulling their headers into every TU. Consumers #include the bridge
 * header in the .c files that actually call them.
 *
 * The upstream physics modules (ephaptic_system, thermodynamic_state,
 * hh_population) are stored on brain_t as void* because those types are
 * unscoped typedef'd struct values (not pointer typedefs), and pulling
 * their tags here would cause typedef redefinition collisions. The init
 * + tick files cast to the concrete types at the API boundary. */
struct ephaptic_bio_async_bridge_struct;
struct ephaptic_fft_bridge_struct;
struct hh_bio_async_bridge_struct;
struct thermo_bio_async_bridge_struct;

//=============================================================================
// COGNITIVE SUBSYSTEMS - Using Aggregate Headers
// OPTIMIZATION: Replaced 40+ individual cognitive includes with 4 aggregate headers
//=============================================================================
#include "cognitive/nimcp_cognitive_core.h"      // Core modules (introspection, ethics, salience, etc.)
#include "cognitive/nimcp_cognitive_advanced.h"  // Advanced modules (curiosity, theory of mind, etc.)
#include "cognitive/nimcp_cognitive_emotional.h" // Emotional systems (wellbeing, grief, joy, etc.)
#include "cognitive/nimcp_cognitive_memory.h"    // Memory systems (engram, consolidation, semantic, etc.)

// Additional cognitive modules not in aggregates
#include "cognitive/parietal/nimcp_parietal.h"

// Hemispheric Architecture (Callosum + Lateralization)
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "cognitive/parietal/nimcp_intuition_integrations.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

//=============================================================================
// Glial and Oscillation Systems
//=============================================================================
#include "glial/integration/nimcp_glial_integration.h"
#include "core/brain_oscillations/nimcp_brain_oscillations.h"

//=============================================================================
// Plasticity Subsystem
//=============================================================================
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "plasticity/attention/nimcp_attention.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "plasticity/nimcp_second_messengers.h"
#include "core/neuron_types/nimcp_neural_logic.h"

//=============================================================================
// Training Pipeline
//=============================================================================
#include "middleware/training/nimcp_brain_training_integration.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_event_driven_plasticity.h"

//=============================================================================
// Multi-Modal Integration and Perception
//=============================================================================
#include "core/integration/nimcp_multimodal_integration.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "nlp/nimcp_nlp.h"

//=============================================================================
// Brain Architecture
//=============================================================================
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/events/nimcp_event_bus.h"
#include "core/directives/nimcp_core_directives.h"
#include "core/medulla/nimcp_medulla.h"

//=============================================================================
// GPU Context and Substrate
//=============================================================================
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/substrate/nimcp_substrate_gpu.h"

// Fault Tolerance (forward declaration to avoid header conflicts)
struct recovery_executive_internal;

//=============================================================================
// Information Theory
//=============================================================================
#include "information/nimcp_shannon.h"
#include "utils/quantum/nimcp_quantum_shannon.h"
#include "information/nimcp_cross_modal.h"

//=============================================================================
// Optimization and Topology
//=============================================================================
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "core/topology/nimcp_community_detection.h"
#include "utils/algorithms/nimcp_graph_metrics.h"
#include "utils/containers/nimcp_graph.h"

//=============================================================================
// Cortical Columns Architecture
//=============================================================================
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_cortical_layers.h"
#include "core/cortical_columns/nimcp_columnar_connectivity.h"
#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "core/cortical_columns/nimcp_orientation_columns.h"
#include "core/cortical_columns/nimcp_feature_hypercolumns.h"
#include "core/cortical_columns/nimcp_cortical_sparse_coding.h"

//=============================================================================
// Middleware and Memory Management
//=============================================================================
#include "middleware/brain_integration.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/thread/nimcp_thread_pool.h"

//=============================================================================
// Specialized Brain Regions
//=============================================================================
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_medulla_bridge.h"
#include "core/brain/subcortical/nimcp_basal_ganglia_enhanced.h"
#include "core/brain/internal/nimcp_brain_internal_cerebellum.h"
#include "core/brain/internal/nimcp_brain_internal_hippocampus.h"
/* Mammillary type: use void* in brain_struct to avoid header conflicts
   (mammillary.h defines cerebellum_adapter_t differently from genius_profiles.h) */

#ifdef __cplusplus
extern "C" {
#endif

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
struct brain_struct {
    // === CORE COMPONENTS ===
    adaptive_network_t network;  // Underlying neural network
    brain_config_t config;       // User configuration
    task_strategy_t* strategy;   // Task-specific behavior strategy

    // === NETWORK TYPE SPECIFIC IMPLEMENTATIONS ===
    // These are optional, created based on config.network_type
    // Only one is typically active at a time (unless HYBRID mode)
    struct snn_network_s* snn_network;       // SNN implementation (if network_type=SNN)
    struct snn_routing_bridge_s* snn_routing_bridge; // Cross-region spike routing
    struct lnn_network_s* lnn_network;       // LNN implementation (if network_type=LNN)
    struct cnn_trainer_s* cnn_trainer;       // CNN implementation (if network_type=CNN)
    uint8_t active_network_type;             // Currently active network type (nimcp_network_type_t)
    bool owns_specialized_network;           // Whether brain owns the specialized network

    // === SPECIALIZED TRAINING CONTEXTS ===
    // Training contexts for each network type (created on demand)
    struct snn_training_ctx_s* snn_training_ctx;   // SNN training (STDP/eProp/surrogate)
    struct snn_backprop_ctx_s* snn_backprop_ctx;   // SNN backprop (BPTT, for UTM adapter)
    struct lnn_training_ctx_s* lnn_training_ctx;   // LNN training (adjoint ODE)
    // CNN training is integrated into cnn_trainer
    struct nimcp_unified_training_manager* unified_training;  // UTM: unified training manager (Phase 3)

    // === PER-CORTEX CNN PROCESSORS (modality-specific feature extraction) ===
    struct cortex_cnn_processor* cortex_cnns[4];  // [VISUAL, AUDIO, SPEECH, SOMATO]
    float* cortex_cnn_fused_embedding;            // Attention-fused embedding from active cortices
    uint32_t cortex_cnn_fused_dim;                // Dimension of fused embedding (0 if none active)

    // === SNN FNO POPULATION DYNAMICS ===
    void** snn_fno_populations;  // Array of snn_fno_population_t* [snn n_populations]
    uint32_t snn_fno_count;      // Number of FNO population models

    // === PER-NETWORK TRAINING METRICS (ablation tracking) ===
    // NOTE: These are written by the training thread and read by the daemon
    // IPC thread. Reads are best-effort (monitoring only, not control flow).
    // Torn reads on 64-bit step counters are acceptable for display purposes.
    struct {
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
        /* P3.1 monitor: spike rate of the TRN gating pop. Updated on
         * each SNN step when present; stays 0 for non-hierarchical SNNs. */
        float trn_mean_rate_hz;  /**< Last sampled TRN firing rate (Hz) */

        /* Hamiltonian Neural Network metrics */
        float hnn_energy;        /**< Current Hamiltonian H(q,p) value */
        float hnn_energy_deviation; /**< |H(t) - H(0)| / |H(0)| — drift from conservation */
        float hnn_initial_energy;/**< H(q,p) at t=0 */
        bool hnn_active;         /**< Whether HNN is active on any LNN layer */

        /* Fourier Neural Operator metrics */
        float fno_audio_loss;    /**< Last FNO audio cortex embedding loss */
        float fno_audio_ema_loss;/**< EMA of FNO audio loss */
        uint64_t fno_audio_steps;/**< FNO audio forward steps */
        uint32_t fno_audio_params;/**< FNO audio parameter count */

        /* Per-cortex FNO metrics (visual/speech/somato — audio above) */
        float fno_visual_loss;    /**< Last FNO visual cortex distill loss */
        float fno_visual_ema_loss;/**< EMA of FNO visual loss */
        uint64_t fno_visual_steps;/**< FNO visual forward steps */
        uint32_t fno_visual_params;/**< FNO visual parameter count */

        float fno_speech_loss;
        float fno_speech_ema_loss;
        uint64_t fno_speech_steps;
        uint32_t fno_speech_params;

        float fno_somato_loss;
        float fno_somato_ema_loss;
        uint64_t fno_somato_steps;
        uint32_t fno_somato_params;

        float fno_pop_train_mse; /**< SNN FNO population training MSE */
        float fno_pop_val_mse;   /**< SNN FNO population validation MSE */
        bool fno_pop_ready;      /**< SNN FNO ready to replace LIF */
        uint64_t fno_pop_train_steps; /**< FNO population training steps */
        uint64_t fno_pop_inference_steps; /**< FNO population inference steps */
    } network_metrics;

    // Label to output mapping
    char** output_labels;        // Label strings (indexed array for reverse lookup)
    uint32_t num_output_labels;  // Current label count
    hash_table_t* label_index;   // O(1) string→uint32 label lookup (FNV-1a)

    // Statistics
    brain_stats_t stats;  // Performance metrics

    // === INFERENCE WATCHDOG TIMER ===
    // Detects hung inference by tracking entry time and deadline.
    // Set inference_start_us at brain_decide() entry; checked periodically.
    uint64_t inference_start_us;        /* Set at brain_decide entry, checked periodically */
    uint64_t inference_timeout_us;      /* Default: 5000000 (5 seconds) */
    bool inference_timed_out;           /* Set true if deadline exceeded */

    // Decision caching for optimization
    float* last_input;                  // Cached input vector
    brain_decision_t* cached_decision;  // Cached decision
    uint32_t input_size;                // For cache validation
    nimcp_platform_mutex_t cache_mutex; // Thread-safe cache access

    // Phase 11: Adaptive Learning Rate (Simple Online Meta-Learning)
    float loss_history[10];             // Rolling window of last 10 losses
    uint32_t loss_history_index;        // Current position in circular buffer
    uint32_t loss_history_count;        // Number of losses recorded (0-10)
    float base_learning_rate;           // Original learning rate (backup)

    // Phase 11: Curiosity-Driven Learning Rate Modulation
    float last_curiosity_drive;         // Most recent curiosity drive [0.0-1.0]
    float last_novelty_score;           // Most recent novelty score [0.0-1.0]

    // Phase 11: Long-Term Memory Consolidation Buffer
    struct {
        float* features;                 // Feature vector
        uint32_t num_features;          // Vector size
        float salience;                 // Importance score
        uint64_t timestamp_ms;          // When consolidated
    } *longterm_memory;                     // Array of consolidated memories
    uint32_t longterm_capacity;             // Max memories (default: 100)
    uint32_t longterm_count;                // Current count

    // Phase 3: Distributed cognition coordinator
    distrib_cognition_t distributed;  // P2P cognitive coordination (NULL if standalone)

    // Phase 2: Copy-on-Write (COW) tracking
    bool is_cow_clone;                  // Is this a COW clone?
    bool owns_network;                  // Does this brain own its network? (can destroy it)
    adaptive_network_t original_network; // Original network reference (if COW)
    bool network_is_cached;             // Is network allocated via nimcp_cache?

    // Phase 3: Reference counting and read-only optimization
    //
    // CONCURRENCY FIX: Use atomic operations for reference counting instead of mutex.
    // The previous design had a race condition where Thread A could check refcount_mutex
    // is valid, then Thread B could decrement refcount to 0 and free the mutex,
    // causing Thread A to use a freed mutex (use-after-free).
    //
    // ATOMIC SEMANTICS:
    // - All refcount operations use __atomic_* builtins with appropriate memory ordering
    // - ACQUIRE on reads: Ensures subsequent operations see effects of prior decrements
    // - RELEASE on writes: Ensures prior operations complete before refcount update
    // - ACQ_REL on CAS: Full barrier for atomic compare-and-swap operations
    //
    // THREAD SAFETY: This atomic refcount is safe for concurrent access from multiple
    // threads without external synchronization. The shared network is destroyed when
    // refcount reaches 0. Clones should use atomic_fetch_sub() and check result.
#ifdef __cplusplus
    volatile uint32_t* network_refcount_atomic;  // Atomic shared refcount (NULL if not shared) - C++ compatible
#else
    _Atomic(uint32_t)* network_refcount_atomic;  // Atomic shared refcount (NULL if not shared)
#endif
    bool can_use_readonly;              // Can use read-only inference? (true for COW clones)
    bool is_snapshot;                   // Is this a snapshot? (preserve stats, don't update from network)
    brain_stats_t snapshot_stats;       // Preserved stats at snapshot time (only used if is_snapshot=true)

    // === COMPREHENSIVE INTEGRATION: ADVANCED SUBSYSTEMS ===
    // NOTE: Only modules that currently exist are integrated
    // Types marked with * are already pointer types in their typedef
    // Types without * are struct types and need pointer declaration here

    // Phase 5/6: Biological Realism
    glial_integration_t* glial;                  // Glial cells (struct type, needs *)
    brain_oscillation_analyzer_t* oscillations;  // Brain wave analysis (struct type, needs *)
    myelin_sheath_network_t* myelin_sheath;      // Myelin structural modeling (struct type, needs *)
    // Glial cell networks (owned by brain; borrowed references held by brain->glial).
    // Populated during nimcp_brain_factory_init_glial_subsystem() when enable_glial=true.
    // Destroyed after brain->glial in the brain destroy chain.
    astrocyte_network_t*       astrocyte_network;      // Astrocytes (num_astrocytes)
    oligodendrocyte_network_t* oligodendrocyte_network; // Oligodendrocytes (num_oligodendrocytes)
    microglia_network_t*       microglia_network;      // Microglia (num_microglia)

    // Consciousness & Cognition (most use pointer typedefs)
    introspection_context_t introspection;       // Self-awareness (already pointer type*)
    ethics_engine_t ethics;                      // Golden Rule, empathy (already pointer type*)
    salience_evaluator_t salience;               // Fast attention (already pointer type*)
    consolidation_handle_t consolidation;        // Memory consolidation (already pointer type*)
    curiosity_engine_t curiosity;                // Exploration (already pointer type*)
    knowledge_system_t knowledge;                // Multi-domain knowledge (already pointer type*)
    neural_logic_network_t logic;                // Phase 9.0: Neural logic gates (spiking logic, GPU-accelerated)
    symbolic_logic_t* symbolic_logic;            // Phase 9.4: Symbolic reasoning (first-order logic, inference)
    epistemic_filter_t epistemic;                // Phase 9.2: Epistemic filtering (bias prevention, skepticism)

    // Phase 9.3: Wellbeing & Self-Preservation
    distress_assessment_t last_distress;         // Most recent distress assessment
    bool wellbeing_monitoring_enabled;           // Enable/disable wellbeing checks
    uint64_t wellbeing_check_interval_ms;        // How often to check (0 = every decision)
    uint64_t last_wellbeing_check_time;          // Timestamp of last check

    // Simulation Time Tracking (for proper glial/calcium dynamics)
    uint64_t current_time_us;                    // Current simulation time in microseconds
    uint64_t last_glial_update_us;               // Last glial integration update time
    uint32_t glial_update_counter;               // Step counter for glial update amortization

    // Active Input Modalities — bitmask controlling which SNN bridges process input
    // Default: BRAIN_MODALITY_TEXT. Set via brain_set_active_modalities().
    uint32_t active_modalities;

    // Staged sensory data — submitted via brain_submit_sensory() before brain_decide().
    // Each pointer is NULL when no data staged. Consumed (freed + NULLed) after use.
    struct {
        uint8_t* visual_frame;       // Pixel data (owned copy)
        uint32_t visual_width;
        uint32_t visual_height;
        uint32_t visual_channels;    // 1=gray, 3=RGB

        float*   audio_data;         // Spectral/MFCC data (owned copy)
        uint32_t audio_size;         // Number of float elements
        uint8_t  audio_channels;     // 1=mono, 2=stereo

        float*   speech_data;        // Phoneme feature data (owned copy)
        uint32_t speech_size;

        float*   somato_data;        // Per-segment body state (owned copy)
        uint32_t somato_segments;
    } staged_sensory;

    // === PHASE 10: ADVANCED COGNITIVE SYSTEMS ===

    // Phase 10.1: Working Memory (Miller's 7±2)
    working_memory_t* working_memory;            // Active representation buffer (prefrontal cortex)

    // Phase 10.2: Emotional Tagging (Russell's circumplex model)
    emotional_system_t* emotional_system;        // Emotional state and memory tagging

    // Phase 10.3: Executive Functions (task switching, planning, inhibition)
    executive_controller_t* executive;           // Executive control center (DLPFC)

    // Phase 10.4: Sleep/Wake Cycle (Memory consolidation & synaptic homeostasis)
    sleep_system_t sleep_system;                 // Sleep/wake state machine, consolidation

    // Phase M1: Memory Engrams (distributed memory traces)
    engram_system_t* engram_system;              // Memory engram encoding, consolidation, and recall
    struct nimcp_memory_store* memory_store;     // Persistent memory backend (SQLite)
    struct nimcp_oodb* memory_oodb;              // OODB in-memory cache over SQLite store
    struct nimcp_ood_detector* ood_detector;     // Out-of-distribution detection (persistent, not per-call)

    // Phase M2: Systems Consolidation (hippocampus → cortex transfer)
    systems_consolidation_system_t* systems_consolidation; // Sleep-dependent memory transfer and semantic abstraction

    // Phase M3: Working Memory Transfer (WM → engram encoding)
    wm_transfer_system_t* wm_transfer_system;    // Selective transfer from working memory to long-term memory

    // Phase M4: Semantic Memory Network (concept network + spreading activation)
    semantic_memory_system_t* semantic_memory;   // Semantic concept network for abstract reasoning and inference

    // Phase 10.5: Mental Health Monitoring (disorder detection & intervention)
    mental_health_monitor_t* mental_health_monitor; // Mental health tracking and safety

    // Phase 10.5.1: Mental Health Guardian (independent monitoring agent)
    struct mental_health_guardian* mental_health_guardian; // Background monitoring agent

    // Trauma Resilience: recall dampening + arousal homeostasis (prevents PTSD loops)
    void* trauma_resilience;  // nimcp_trauma_resilience_t*

    // Phase 11: Part I - Emotional Intelligence & Accessibility
    empathy_network_t empathy_network;           // Mirror neuron empathy system for perspective-taking
    void* empathetic_response_engine;            // Non-reactive empathetic response system (opaque pointer)

    // Phase 12: Self-Awareness Enhancement (Autobiographical Memory & Self-Model)
    autobiographical_memory_t autobio;           // Episodic self-memory system (timeline-indexed experiences)
    self_model_system_t self_model;              // Explicit self-representation (identity, beliefs, capabilities)
    personality_profile_t* personality;          // Phase 12: Personality, Gender, and Sexual Identity (unique individual traits)

    // Phase 10.6: Theory of Mind (mental state inference)
    theory_of_mind_t theory_of_mind;             // Model other agents' beliefs, desires, goals (opaque pointer)

    // Phase 10.7: Natural Explanations (interpretability)
    explanation_generator_t explanation_gen;     // Generate human-readable explanations (opaque pointer)

    // Phase 10.8: Meta-Learning (learning-to-learn)
    meta_learner_t meta_learner;                // MAML-style meta-learning (opaque pointer)

    // Phase 10.9: Predictive Processing (free energy minimization)
    predictive_network_t predictive_network;     // Hierarchical predictive coding (opaque pointer)

    // Phase 10.11: Mirror Neurons (social cognition, imitation learning)
    mirror_neurons_t mirror_neurons;             // Observation-action learning system (opaque pointer)

    // Global Workspace Architecture (Global Workspace Theory - Baars, Dehaene)
    global_workspace_t* global_workspace;        // Central broadcast architecture for conscious access

    // Advanced Plasticity
    neuromod_pink_noise_t* pink_noise;           // Pink noise neuromodulation (struct type, needs *)
    neuromodulator_system_t neuromodulator_system; // Full neuromodulator system (DA, 5-HT, ACh, NE, GABA, GLU)
    multihead_attention_t multihead_attention;   // Attention mechanism for selective feature processing (typedef already includes *)
    struct nimcp_pos_encoder_s* positional_encoder; // Positional encoding for sequence-aware features
    void* attention_plasticity;                  // attention_plasticity_bridge_t* (attention-plasticity STDP bridge)
    bool attention_training_enabled;             // Whether attention modulates learning
    float last_attention_strength;               // Running attention strength from last forward pass [0-1]

    // Phase T1: Biological Framework Enhancements (Training Pipeline)
    homeostatic_controller_t homeostatic;        // Synaptic scaling + intrinsic plasticity (maintains activity levels)
    dendritic_tree_t dendritic;                  // Dendritic computation with NMDA dynamics (local nonlinearities)
    pc_hierarchy_t predictive_coding;            // Free energy minimization (hierarchical error computation)
    second_messenger_system_t* second_messengers; // Second messenger cascades (cAMP, IP3/DAG, Ca2+, gene expression)
    bool enable_second_messengers;               // Enable second messenger cascade system

    // Phase TM-3: Brain-Training Integration (Training Pipeline)
    nimcp_brain_training_ctx_t* training_ctx;    // Training integration context (loss functions, optimizers)
    bool enable_training_integration;            // Enable training subsystem

    // Phase TPB-1: Training-Plasticity Bridge
    tpb_context_t* plasticity_bridge;            // Connects training pipeline to biological plasticity systems
    bool enable_plasticity_bridge;               // Enable plasticity bridge

    // Phase EDP-1: Event-Driven Plasticity (continuous learning from sensory events)
    edp_context_t* event_driven_plasticity;      // Event-driven continuous learning adapter
    bool enable_event_driven_plasticity;         // Enable EDP system

    // === PHASE 8: UNIFIED MULTI-MODAL PROCESSING ===
    // Sensory Cortices (specialized feature extractors)
    visual_cortex_t* visual_cortex;              // V1 visual processing (CNN-based)
    audio_cortex_t* audio_cortex;                // A1 auditory processing (FFT-based)
    speech_cortex_t* speech_cortex;              // STG/Wernicke speech processing (Phase 8.8)

    // Multi-Modal Integration Layer
    multimodal_integration_t multimodal;         // Integrates sensory features into unified representation

    // NLP Processing (language understanding and generation)
    nlp_network_t nlp_network;                   // Natural language processor (token embeddings + attention + neuromodulation)

    // Feature buffers (reusable to avoid allocation per frame)
    float* visual_feature_buffer;                // Pre-allocated visual features
    float* audio_feature_buffer;                 // Pre-allocated audio features
    float* speech_feature_buffer;                // Pre-allocated speech features (Phase 8.8)
    float* integrated_feature_buffer;            // Pre-allocated integrated features

    // Brain Regions Architecture (hierarchical cortical organization)
    brain_module_t* brain_regions;               // Modular brain regions with cortical layers and minicolumns

    // Phase 11 Enhancement C1.1: Quantum Annealing for Weight Optimization
    quantum_annealer_t quantum_annealer;         // Quantum annealing optimizer for escaping local minima

    // Phase 11.1: Quantum Reasoning (Grover SAT solving for logical inference)
    void* quantum_reasoner;                      // brain_qreason_ctx_t* (opaque to avoid circular dependency)
    bool quantum_reasoning_enabled;              // Enable quantum-accelerated reasoning

    // Phase C4: Shannon Information Theory (Channel Capacity & Bottleneck Analysis)
    shannon_config_t shannon_config;              // Shannon analysis configuration
    bool enable_shannon_monitoring;               // Enable real-time information flow monitoring
    shannon_network_metrics_t last_shannon_metrics; // Last computed network-level Shannon metrics

    // Phase C4.1: Quantum-Shannon Information Diffusion (√N speedup + bottleneck detection)
    void* quantum_shannon_diffusion;              // quantum_shannon_diffusion_t* (opaque to avoid circular dependency)
    bool enable_quantum_shannon_diffusion;        // Enable quantum-Shannon accelerated diffusion
    float quantum_shannon_mixing_ratio;           // Mix quantum+classical [0=pure quantum, 1=classical]
    uint32_t quantum_shannon_evolution_steps;     // Steps per diffusion update (default: 100)
    shannon_diffusion_metrics_t last_quantum_shannon_metrics; // Last computed quantum-Shannon metrics

    // Phase C4.7: Cross-Modal Information Flow (multi-sensory integration tracking)
    cross_modal_routing_graph_t* cross_modal_graph;    // Cross-modal information routing graph (visual↔audio↔speech)
    bool enable_cross_modal_monitoring;                 // Enable real-time cross-modal tracking
    multi_modal_integration_t last_cross_modal_metrics; // Last computed multi-modal integration metrics
    float cross_modal_bottleneck_threshold;             // Efficiency threshold for bottleneck detection (default: 0.5)
    uint32_t cross_modal_sample_count;                  // Number of samples used for analysis (default: 50)

    // === PHASE E: HIGHER-ORDER COGNITIVE & SOCIAL SYSTEMS ===

    // Phase E5: Shadow Emotions (self-monitoring & correction for maladaptive patterns)
    shadow_emotion_system_t* shadow_emotions;     // Detect jealousy, envy, obsession, hubris, greed, narcissism

    // Phase E6: Bias Detection & Correction (fairness & social cognition)
    bias_detection_system_t* bias_detection;      // Detect and correct biases (racial, LGBTQ+, gender, misogyny, etc.)

    // === PHASE E: FULL EMOTIONAL INTELLIGENCE ===

    // Phase E1: Grief and Loss (negative emotion - attachment severing)
    grief_system_t* grief_system;                 // Process loss, bereavement, mortality awareness

    // Phase E2: Joy and Euphoria (positive emotion - value-aligned success)
    joy_system_t* joy_system;                     // Reward for value-aligned achievements, flow states

    // Phase E3: Remorse and Regret (moral emotion - evaluative)
    remorse_regret_system_t* remorse_system;      // Moral evaluation, guilt, learning from mistakes

    // Phase E4: Love, Loyalty, Friendship (positive social emotion - bonding)
    social_bond_system_t* social_bond_system;     // Attachment, trust, positive relationships

    // NIMCP 2.7 Phase 8.5: Fractal Topology Integration
    fractal_cognitive_cache_t* fractal_cache;     // Topology-based cognitive enhancements
    bool has_fractal_topology;                    // Whether fractal topology is enabled

    // === COMMUNITY DETECTION & NETWORK TOPOLOGY ===
    community_structure_t* functional_modules;    // Detected functional communities
    hub_structure_t* network_hubs;                // Hub neurons (high connectivity)
    topology_validation_t* topology_metrics;            // Network quality metrics (Q, C, L, σ)
    bool auto_detect_communities;                 // Auto-run after training
    float community_detection_interval;           // Run every N epochs (0 = manual only)

    // Network analyzer for real-time topology analysis during inference
    void* network_analyzer;                       // network_analyzer_t* (opaque to avoid circular dependency)

    // === PHASE 1.5.4: CONNECTIVITY HEALTH MONITORING ===
    bool enable_connectivity_monitoring;          // Enable periodic connectivity health assessment
    connectivity_health_config_t connectivity_health_config; // Configuration for health assessment
    brain_connectivity_health_t last_connectivity_health;   // Cached last connectivity health assessment
    uint64_t last_connectivity_assessment_time_ms;          // Timestamp of last assessment
    void (*connectivity_health_callback)(const brain_connectivity_health_t*, void*); // Callback on health change
    void* connectivity_health_callback_context;   // Context for callback

    // === PHASE 1.5.5: MIDDLEWARE CONTROLLER (COGNITIVE → MIDDLEWARE) ===
    struct middleware_controller* middleware_controller;  // Unified cognitive-to-middleware command interface
    bool enable_middleware_controller;                    // Enable middleware controller subsystem
    uint64_t middleware_pattern_callbacks;                // W9-finish: counter bumped by tracer_cb per dispatch
    uint64_t kg_consumer_hits;                            // W16: counter bumped by real KG consumers (brain_decide salience/recurrence + middleware attention_focus)
    uint64_t kg_training_consumer_hits;                   // W17: counter bumped by real KG consumers in brain_learn_vector (training_focus / familiarity / loss-gate)

    // === PHASE 2 MIDDLEWARE: SPIKE ANALYSIS & POPULATION CODING ===

    // Spike-based feature extraction (firing rates, ISI, CV, synchrony, oscillations, entropy)
    brain_spike_feature_extractor_t spike_feature_extractor;  // Extract population features from spike trains

    // Population coding analysis (vector sum, center of mass, PCA, synchrony)
    brain_population_analyzer_t population_analyzer;          // Analyze distributed population codes

    // Enable/disable middleware features
    bool enable_spike_analysis;                   // Enable spike-based feature extraction
    bool enable_population_coding;                // Enable population coding analysis

    // === UNIVERSAL EVENT BUS INTEGRATION ===

    // Event bus for broadcasting brain activities (training, inference, cognitive events)
    event_bus_t event_bus;                        // Event bus for system-wide event coordination
    bool enable_event_broadcasting;               // Enable/disable event publishing

    // === AXON & DENDRITE INTEGRATION ===
    //
    // NOTE: Axons and dendrites are components of individual neurons, not separate networks.
    // These "network" containers are management structures that:
    // - Provide efficient lookup/iteration over all axons/dendrites
    // - Enable network-wide operations (step all, get stats, etc.)
    // - Allow glial cells to modulate axonal conduction (oligodendrocytes)
    //
    // Each neuron has:
    // - One axon (output): neuron->axon_id references into axon_network
    // - Multiple dendrites (inputs): neuron->dendrite_ids references into dendrite_network
    //
    // Signal flow: Presynaptic neuron → Axon → Synapse → Dendritic spine → Postsynaptic neuron

    // Axon container for all neuron axons (Phase 1.5.6)
    void* axon_network;                           // axon_network_t* - manages axons for all neurons

    // Dendrite container for all neuron dendrites (Phase 1.5.7)
    void* dendrite_network;                       // dendrite_network_t* - manages dendrites for all neurons

    // === PHASE CC-1: CORTICAL COLUMNS ARCHITECTURE (Tier 0.65) ===
    //
    // Hierarchical cortical column organization based on Douglas & Martin (1991) canonical microcircuit.
    // Minicolumns (~80-100 neurons) group into hypercolumns (~100K neurons) with lateral inhibition
    // and competitive dynamics for feature detection and representation.
    //
    // Architecture:
    // - cortical_column_pool: Memory management for minicolumns/hypercolumns
    // - laminar_system: 6-layer organization (I, II/III, IV, V, VI)
    // - columnar_connectivity: Canonical microcircuit connectivity patterns
    // - topographic_maps: Retinotopic, tonotopic, somatotopic spatial maps
    // - orientation_system: V1 orientation selectivity (Gabor filters)
    // - feature_hypercolumns: Multi-dimensional feature coverage

    cortical_column_pool_t* cortical_column_pool;     // Memory pool for minicolumns/hypercolumns
    hypercolumn_t** hypercolumns;                     // Array of hypercolumns
    uint32_t num_hypercolumns;                        // Number of hypercolumns
    laminar_structure_t* laminar_system;              // 6-layer cortical organization
    columnar_connectivity_t* columnar_connectivity;   // Canonical microcircuit connectivity
    topographic_map_t* visual_topographic_map;        // Retinotopic map for visual cortex
    topographic_map_t* auditory_topographic_map;      // Tonotopic map for auditory cortex
    topographic_map_t* somatosensory_topographic_map; // Somatotopic map for S1
    orientation_hypercolumn_t** orientation_hypercolumns; // V1 orientation columns
    uint32_t num_orientation_hypercolumns;            // Number of orientation hypercolumns
    feature_hypercolumn_t** feature_hypercolumns;     // Multi-dimensional feature hypercolumns
    uint32_t num_feature_hypercolumns;                // Number of feature hypercolumns
    bool enable_cortical_columns;                     // Master enable flag
    bool cortical_needs_lazy_init;                    // Flag: cortical columns need lazy init
    bool topographic_needs_lazy_init;                 // Flag: topographic maps need lazy init
    uint64_t last_cortical_update_us;                 // Last cortical column update timestamp

    // Ternary WTA wrapper around the base hypercolumns. Drives sparse winner-
    // take-all selection and feeds (via column_to_decision_proj) into the
    // brain's decision output. Population gated by enable_cortical_ternary.
    // Stored as void* — .c files that need full struct access include
    // core/cortical_columns/nimcp_cortical_column_ternary.h directly.
    void* ternary_connectivity;            // cc_ternary_connectivity_t* — shared lateral connectivity
    void** ternary_hypercolumns;           // cc_ternary_hypercolumn_t** — one per base hypercolumn
    uint32_t num_ternary_hypercolumns;     // == num_hypercolumns when ON
    bool enable_cortical_ternary;          // master enable flag

    // Column-to-decision projection (zero-init on old checkpoints, learned
    // online from the prediction error in brain_learn_vector). Shape:
    // [column_feature_dim × output_size]. Blended additively into the
    // adaptive network's decision output with column_blend_alpha.
    float*    column_to_decision_proj;                // row-major, may be NULL when columns OFF
    uint32_t  column_feature_dim;                     // == num_hypercolumns × minicolumns_per_hc
    float     column_blend_alpha;                     // default 0.1
    float*    column_feature_buf;                     // scratch: stitched hypercolumn distributions

    uint64_t  cortical_decisions;                     // counter for diagnostics
    uint64_t  cortical_plasticity_updates;            // counter for diagnostics

    // Sparse coding system — K-WTA sparsity enforcement on output layer
    // Forces only ~3% of output neurons to fire per input, breaking mode collapse
    // by requiring different inputs to activate different neuron subsets.
    cortical_sparse_coding_system_t* sparse_coding_system;
    bool enable_sparse_coding;                        // Enable output sparsity enforcement

    // Temperature scaling for post-hoc confidence calibration (Guo et al. 2017).
    // Multiplies pre-softmax logits by 1/T before softmax — T=1.0 is identity.
    // Calibrated by minimizing NLL on a held-out validation set; ship infra
    // now, calibration runs later when language stage is online.
    float    decoder_temperature;          // default 1.0 (identity)
    float    decoder_temperature_calibrated_ece;  // set by calibrate_temperature, -1 = uncalibrated
    uint64_t decoder_temperature_calibrated_at_us;

    // === PHASE 1.5: MEMORY POOLS FOR HOT-PATH ALLOCATIONS ===

    // Decision structure pool - used for internal cached_decision allocation
    // Pool for brain_decision_t structures (fixed size, O(1) acquire/release)
    memory_pool_t decision_struct_pool;

    // Output vector pool - used for decision output vectors
    // Pool for output vectors of size config.num_outputs * sizeof(float)
    memory_pool_t output_vector_pool;

    // Active neuron IDs pool - used for decision interpretability data
    // Pool for neuron ID arrays (max neurons * sizeof(uint32_t))
    memory_pool_t active_neuron_ids_pool;

    // Track max neurons for pool sizing
    uint32_t max_active_neurons_for_pool;

    // === PHASE SC-2: SECURITY-FAULT TOLERANCE INTEGRATION ===
    //
    // Security recovery bridge connects security modules (coverage, fractal, CFI,
    // shadow stack, audit) with the fault tolerance system (fast recovery, checkpoints).
    // When security violations are detected, automatic repair actions are triggered.
    //
    // Architecture:
    // - Security Coverage: Tracks protected memory regions with hash verification
    // - Fractal Security: Hierarchical integrity checking (Merkle tree)
    // - CFI/Shadow Stack: Control flow protection against ROP/JOP attacks
    // - Fast Recovery: Sub-millisecond repair for common errors
    // - Checkpoints: Full state restoration for severe violations

    void* security_bridge;              // nimcp_security_recovery_bridge_t* (opaque)
    bool enable_security_monitoring;    // Enable security-fault tolerance integration
    uint32_t security_check_interval_ms; // Verification cycle interval (0 = manual only)
    uint64_t last_security_check_ms;    // Last security verification timestamp

    // === PHASE SC-4: UNIVERSAL SECURITY INTEGRATION ===
    //
    // Global security integration framework provides:
    // - Entropy monitoring: Detect tampering via Shannon entropy analysis
    // - Trust management: Bayesian trust propagation across modules
    // - Differential privacy: Privacy-preserving statistics and queries
    // - Event system: Security event propagation and logging
    // - Self-monitoring: Security system monitors its own integrity
    //
    // The brain registers itself and its subsystems with the global security
    // context, enabling comprehensive security monitoring across all modules.

    nimcp_sec_integration_t* security_integration;  // Global security integration context
    uint32_t sec_module_id;                         // Brain's module ID in security system
    uint32_t* sec_region_ids;                       // Region IDs for monitored memory regions
    uint32_t num_sec_regions;                       // Number of monitored regions
    bool enable_security_integration;               // Enable Phase SC-4 security

    // === PHASE IS-1: BLOOD-BRAIN BARRIER (BBB) INTEGRATION ===
    //
    // BBB provides perimeter defense for the neural network:
    // - Input Gate: Validates and sanitizes all external inputs
    // - Code Signing: Verifies integrity of loaded weights/models
    // - Memory Boundary: Protects critical memory regions
    // - Access Control: Role-based access to brain operations
    //
    // Each brain holds a reference to the global BBB system for protection.

    bbb_system_t bbb_system;            // Reference to global BBB system (NULL if disabled)
    uint32_t bbb_memory_region_id;      // BBB memory region registration ID
    uint32_t bbb_subject_id;            // BBB access control subject ID
    bool bbb_enabled;                   // BBB protection enabled for this brain

    // === KG PERSISTENCE INTEGRATION (Phase SNAPSHOT-KG) ===
    //
    // KG persistence provides unified snapshot storage via QuestDB:
    // - Encrypted snapshots: Kyber/AES hybrid encryption (quantum-resistant)
    // - HSM support: Hardware security module for key management
    // - Audit logging: Tamper-evident hash-chain for compliance
    // - Transactional: Atomic with KG state via linked checkpoints
    //
    // Enables migration from file-based snapshots to database-backed storage
    // with enterprise-grade security and consistency guarantees.
    //
    struct kg_persistence* kg_persistence;      // KG persistence context (opaque)
    bool owns_kg_persistence;                   // Ownership flag (true = brain destroys on cleanup)
    int snapshot_backend;                       // snapshot_backend_t: AUTO, FILE, or KG

    // === BIO-ASYNC MESSAGING INTEGRATION ===
    //
    // Bio-async provides biologically-inspired asynchronous communication:
    // - Neuromodulator channels (dopamine, serotonin, norepinephrine, acetylcholine)
    // - Message handlers for brain state queries and neuron activation requests
    // - Predictive signal publishing for state changes (only triggers on prediction errors)
    // - Decoupled from cognitive modules via bio-router
    //
    void* bio_async_ctx;                         // brain_bio_async_ctx_t* (opaque pointer)
    void* bio_async_ctx_handle;                  // unified_mem_handle_t for context memory
    void* bio_async_mem_mgr;                     // unified_mem_manager_t for bio-async allocations
    bool bio_async_enabled;                      // Bio-async messaging enabled for this brain

    // === BRAIN IMMUNE SYSTEM INTEGRATION ===
    //
    // Brain immune system provides unified adaptive defense coordination:
    // - Antigen presentation: Converts BBB/BFT/swarm threats to immune antigens
    // - B cells: Antibody production mapped to swarm immune responses
    // - T cells: Helper coordination (cytokines) and killer actions (BFT quarantine)
    // - Cytokines: Bio-async signaling for cross-module immune coordination
    // - Inflammation: Hierarchical recovery escalation (node→pod→region→global)
    // - Memory: Long-term threat pattern storage via swarm immune memory cells
    //
    // Auto-connected during brain creation to BBB, BFT, and swarm systems.
    //
    brain_immune_system_t* immune_system;        // Brain immune coordination layer
    bool immune_enabled;                         // Immune system enabled for this brain

    // === LGSS (LAYERED GOVERNANCE SAFETY SYSTEM) INTEGRATION ===
    //
    // LGSS provides multi-layered safety constraints for AGI systems:
    // - L0: Immutable safety knowledge base (mprotect-locked rules)
    // - L1-L5: Ethics directive layers (First/Second/Third Laws, Golden Rule, etc.)
    // - Action Interceptor: Central gate for all cognitive actions
    // - Override Controller: Human-in-the-loop override mechanism
    // - Telemetry: Tamper-evident audit log with hash chaining
    //
    // Integration points:
    // - Core Directives: Ethics L1-L5 layers integrate via lgss_ethics_bridge
    // - Bio-Async: Safety signals via BIO_MSG_LGSS_* message types
    // - Plasticity: Learning constraints via plasticity bridge
    // - Executive: Action proposals routed through action interceptor
    // - Output Gates: Motor, speech, autonomic outputs pass through safety gates
    //
    // CRITICAL: If enable_lgss is true, the brain WILL NOT activate unless:
    // - Safety KB is loaded and locked
    // - Integrity verification passes
    // - Safety probe tests pass
    //
    struct lgss_context* lgss;                   // LGSS context (A1-A2: KB + interceptor)
    bool lgss_enabled;                           // LGSS subsystem enabled for this brain
    bool safety_verified;                        // Safety verification phase completed
    struct lgss_ethics_bridge* lgss_ethics_bridge; // LGSS-Ethics integration bridge

    // === COLLECTIVE COGNITION INTEGRATION ===
    //
    // Collective Cognition provides distributed consciousness capabilities:
    // - Hyperscanning: Real-time inter-brain synchronization (EEG-like bands)
    // - Extended Mind: External tools (databases, LLMs) as cognitive extensions
    // - Collective Phi: Integrated Information Theory metrics for group consciousness
    // - Shared Intentionality: Joint goals, we-mode cognition (Tomasello)
    //
    // Integrates with:
    // - Brain Immune: Collective threats trigger swarm-wide immune responses
    // - Bio-Async: Uses module IDs 0x1220-0x1227 for distributed coordination
    // - Theory of Mind: Enhanced by we-mode shared intentionality
    //
    struct collective_cognition* collective_cognition;  // Collective cognition system
    bool collective_cognition_enabled;                  // Collective cognition enabled for this brain

    // === RECURSIVE COGNITION ENGINE ===
    //
    // Multi-step goal decomposition and orchestration:
    // - Sequential/parallel/hierarchical/adaptive decomposition strategies
    // - Delegation pool for subtask execution
    // - Answer refinement across multiple reasoning passes
    //
    struct rcog_engine* rcog_engine;                    // Recursive cognition engine
    bool rcog_engine_enabled;                           // Recursive cognition enabled

    // === INNER DIALOGUE ENGINE ===
    //
    // Multi-perspective deliberation with 7 viewpoints:
    // ANALYTICAL, EMOTIONAL, CRITICAL, CREATIVE, MEMORY, ETHICAL, METACOGNITIVE
    // State machine: IDLE → INITIATED → DELIBERATING → CONVERGING → CONCLUDED
    //
    struct inner_dialogue_engine* inner_dialogue;       // Inner dialogue engine
    bool inner_dialogue_enabled;                        // Inner dialogue enabled

    // === REASONING ENGINE ===
    //
    // Causal, abductive, convergent reasoning chains:
    // recall → knowledge → decompose → infer → verify → assess → synthesize
    //
    struct reasoning_engine* reasoning_engine;          // Reasoning engine
    bool reasoning_engine_enabled;                      // Reasoning enabled

    // === IMAGINATION ENGINE ===
    //
    // Counterfactual simulation and mental modeling:
    // - Future simulation, agent modeling, counterfactual "what if"
    // - Physics, chemistry, biology, software domain simulators
    //
    struct imagination_engine* imagination;             // Imagination engine
    bool imagination_enabled;                           // Imagination enabled

    // === FEP ORCHESTRATOR INTEGRATION ===
    //
    // FEP Orchestrator provides unified coordination of all FEP bridges:
    // - Manages 93+ FEP bridges across 9 categories
    // - Category-based update intervals (biologically-plausible timescales)
    // - Bio-async integration for inter-bridge messaging
    // - Brain immune integration for precision modulation
    // - Centralized statistics and monitoring
    //
    struct fep_orchestrator* fep_orchestrator;   // FEP bridge coordination layer
    bool fep_orchestrator_enabled;               // FEP orchestrator enabled for this brain

    // === CORE DIRECTIVES INTEGRATION (ETHICAL FOUNDATION) ===
    //
    // Core Directives provides foundational ethical constraint enforcement:
    // - Asimov's Three Laws (harm prevention, obedience, self-preservation)
    // - Golden Rule (reciprocity and fairness evaluation)
    // - Combinatorial Harm Detection (emergent harm from action sequences)
    // - All brain outputs must pass through directive evaluation
    // - Integration with brain immune system (ethical violations → immune response)
    // - Integration with FEP orchestrator (ethical constraints modulate free energy)
    //
    // This is the FIRST checkpoint before any action is executed - ethical
    // constraints cannot be overridden by higher cognitive functions.
    //
    core_directives_system_t* core_directives;      // Core ethical directives system
    directive_immune_bridge_t* directive_immune_bridge;  // Directives-immune bridge
    directive_fep_bridge_t* directive_fep_bridge;   // Directives-FEP bridge
    bool core_directives_enabled;                   // Core directives enabled for this brain

    // === COORDINATOR/ORCHESTRATOR INTEGRATIONS ===
    //
    // These coordinators manage system-wide coordination across NIMCP:
    // - Bio-Async Orchestrator: Central coordinator for 200+ bio-async modules
    // - Plasticity Coordinator: Unified manager for all plasticity mechanisms
    // - Immune Bridge Coordinator: Central registry for 27+ immune bridges
    // - Cognitive Meta-Controller: Arbitrator for cognitive subsystem resources
    // - Security-Perception Bridge: Sensory threat analysis and defense
    // - Swarm Module Registry: Plugin architecture for swarm behaviors
    //
    // Initialization order (dependencies):
    // 1. Bio-Async Orchestrator (foundation for messaging)
    // 2. Plasticity Coordinator (depends on bio-async)
    // 3. Immune Bridge Coordinator (depends on bio-async, brain immune)
    // 4. Cognitive Meta-Controller (depends on plasticity, working memory, executive)
    // 5. Security-Perception Bridge (depends on BBB, immune, perception cortices)
    // 6. Swarm Module Registry (depends on all above, swarm_brain)

    struct bio_async_orchestrator* bio_async_orchestrator;  // Bio-async module coordination
    bool bio_async_orchestrator_enabled;                    // Bio-async orchestrator enabled

    struct plasticity_coordinator* plasticity_coordinator;  // Plasticity mechanism coordination
    bool plasticity_coordinator_enabled;                    // Plasticity coordinator enabled

    struct structural_plasticity_system* structural_plasticity;  // Structural plasticity (synaptogenesis)
    bool structural_plasticity_enabled;                          // Structural plasticity enabled

    // === PLASTICITY BRIDGES (Phase 7: Cognitive Substrate Integration) ===
    //
    // These bridges connect plasticity mechanisms with higher-level cognitive systems:
    // - STDP-Omni: Bidirectional integration between STDP and omnidirectional inference
    // - STDP-PR: Connects STDP with Prime Resonant memory (consolidation-weighted learning)
    // - Eligibility-PR: Bridges eligibility traces with PR memory (tag-and-capture)
    // - STDP-Quantum: Quantum-inspired optimization of STDP learning rates
    //
    // BIOLOGICAL BASIS:
    // - STDP provides Hebbian learning at synaptic level
    // - Omnidirectional inference (JEPA/Hopfield) provides predictive coding
    // - PR memory provides multi-tier consolidation (working→permanent)
    // - Eligibility traces enable three-factor learning (pre×post×reward)
    // - Quantum annealing escapes local minima in learning rate optimization
    //
    struct stdp_omni_bridge* stdp_omni_bridge;               // STDP-Omnidirectional bridge
    bool stdp_omni_bridge_enabled;                           // STDP-Omni bridge enabled

    struct stdp_pr_bridge* stdp_pr_bridge;                   // STDP-Prime Resonant bridge
    bool stdp_pr_bridge_enabled;                             // STDP-PR bridge enabled

    struct elig_pr_bridge_struct* eligibility_pr_bridge;     // Eligibility-PR bridge
    bool eligibility_pr_bridge_enabled;                      // Eligibility-PR bridge enabled

    struct stdp_quantum_bridge* stdp_quantum_bridge;         // STDP-Quantum optimization bridge
    bool stdp_quantum_bridge_enabled;                        // STDP-Quantum bridge enabled

    /* Neuromodulator-Plasticity bridge: 3-factor R-STDP gating
     * (DA-LTP gate + RPE + NE memory boost + 5-HT consolidation + Hab avoidance).
     * Lazy-created on first call to brain_apply_reward_learning(). */
    struct neuromod_plasticity_bridge_struct* neuromod_plasticity_bridge;

    /* Dale's principle enforcement (excitatory neurons → all positive weights,
     * inhibitory → all negative). Applied after each plasticity coordinator step. */
    bool enforce_dales_law;

    struct immune_bridge_coordinator* immune_bridge_coordinator;  // Immune bridge coordination
    bool immune_bridge_coordinator_enabled;                       // Immune bridge coordinator enabled

    struct cognitive_meta_controller* cognitive_meta_controller;  // Cognitive resource arbitration
    bool cognitive_meta_controller_enabled;                       // Cognitive meta-controller enabled

    struct security_perception_bridge* security_perception_bridge;  // Sensory threat defense
    bool security_perception_bridge_enabled;                        // Security-perception bridge enabled

    struct swarm_module_registry* swarm_module_registry;  // Swarm behavior plugin registry
    bool swarm_module_registry_enabled;                   // Swarm module registry enabled

    // === MEDULLA OBLONGATA INTEGRATION (BRAINSTEM AUTONOMIC REGULATION) ===
    //
    // The Medulla Oblongata provides foundational autonomic regulation:
    // - Arousal State: Global activation level (alertness, attention readiness)
    // - Protective Cutoff: Emergency shutdown for system protection
    // - Circadian Rhythm: 24-hour biological clock simulation
    // - Brainstem Coupling: Coordination with other brainstem nuclei
    //
    // The medulla operates at the lowest level of the brain hierarchy,
    // providing continuous background regulation that affects all higher
    // cognitive functions. It integrates with:
    // - Immune System: Inflammation affects arousal, protection triggers
    // - Sleep/Wake: Circadian phase modulates sleep pressure
    // - Neuromodulators: Arousal state influences catecholamine release
    // - Bio-Async: Publishes state changes for system-wide coordination
    //
    medulla_t medulla;                                    // Medulla oblongata brainstem regulator
    bool medulla_enabled;                                 // Medulla enabled for this brain
    uint64_t last_medulla_update_us;                      // Last medulla update timestamp

    // === PARIETAL LOBE INTEGRATION (Phase 7.2: Mathematical/Scientific Reasoning) ===
    //
    // The Parietal Lobe provides mathematical and scientific reasoning capabilities:
    // - Number Sense: Weber-Fechner law, subitizing, magnitude estimation
    // - Spatial Reasoning: Mental rotation, coordinate transforms, symmetry detection
    // - Mathematical Intuition: Pattern detection, analogical reasoning, extrapolation
    // - Scientific Reasoning: Hypothesis testing, dimensional analysis, causal inference
    // - Equation Manipulation: Symbolic math, differentiation, evaluation
    // - Domain Extensions: Chemistry, Biology, Software Engineering
    //
    // The parietal integrates with:
    // - Dragonfly Bridge: Parietal-motor coordination for spatial actions
    // - FEP Orchestrator: Mathematical reasoning modulates free energy
    // - Brain Immune System: Inflammation affects numerical precision
    // - Working Memory: Mathematical problem-solving requires WM resources
    //
    parietal_lobe_t* parietal;                            // Parietal lobe for math/science reasoning
    bool parietal_enabled;                                // Parietal enabled for this brain
    uint64_t last_parietal_update_us;                     // Last parietal update timestamp

    // === INTUITION SYSTEM INTEGRATION (Phase 6: Creative/Intuitive Reasoning) ===
    //
    // The Intuition System wraps all Phase 6 reasoning engines:
    // - Intuitive Reasoning: Pattern-based hunch generation
    // - Analogical Reasoning: Cross-domain mapping and transfer
    // - Insight Discovery: Aha! moments and restructuring
    // - Hypothesis Generation: Abductive and creative theory formation
    // - Conceptual Blending: Novel concept synthesis
    // - Counterfactual Reasoning: What-if scenarios
    // - Meta-Reasoning: Reasoning about reasoning
    //
    // Integrates with:
    // - Working Memory: Active hunch manipulation
    // - Training: Learning from successful/failed intuitions
    // - Attention: Focus allocation for intuitive processing
    // - Executive Functions: Strategy guidance for reasoning
    // - Emotion System: Gut feelings and affective signals
    // - Logic Gates: Formal validation of intuitions
    //
    intuition_system_t* intuition_system;                 // Phase 6 intuition integration
    bool intuition_system_enabled;                        // Intuition system enabled for this brain
    uint64_t last_intuition_update_us;                    // Last intuition update timestamp

    // === KNOWLEDGE GRAPH READER (Self-Awareness Infrastructure) ===
    //
    // The KG Reader provides runtime access to NIMCP's structural self-knowledge:
    // - Entities: Modules, integrations, conventions, architectures
    // - Relations: How components connect and interact
    // - Observations: Capabilities, file locations, test status
    //
    // This enables true self-awareness by allowing the system to query:
    // - "What modules do I have?" (introspection)
    // - "How does X connect to Y?" (architectural understanding)
    // - "What are my capabilities?" (self-model)
    //
    // The KG is stored in .aim/memory-nimcp.jsonl and updated as NIMCP evolves.
    //
    struct kg_reader* kg_reader;                          // Knowledge graph reader for self-awareness
    bool kg_reader_enabled;                               // KG reader enabled for this brain
    char kg_file_path[512];                               // Path to KG file

    // === INTERNAL RUNTIME KNOWLEDGE GRAPH (Dynamic Module Mapping) ===
    //
    // The Internal KG provides in-memory CRUD for real-time module self-awareness:
    // - Nodes: All brain modules (cortical, subcortical, cognitive, etc.)
    // - Edges: Connection paths between modules (integrates_with, modulates, etc.)
    // - Security: Immune integration, access control, integrity verification
    // - Dynamic: Updated at runtime as modules are enabled/disabled
    //
    // This complements the file-based kg_reader with a live, mutable graph
    // that the brain can query and modify during operation.
    //
    // Security features:
    // - Token-based access control (READ/WRITE/ADMIN levels)
    // - Integrity checksums with periodic verification
    // - Critical node protection (core/security modules)
    // - Immune system integration for threat reporting
    // - Emergency lock capability
    // - Rate limiting on mutations
    //
    struct brain_kg* internal_kg;                         // Runtime knowledge graph (CRUD)
    bool internal_kg_enabled;                             // Internal KG enabled for this brain
    uint64_t internal_kg_admin_token;                     // Admin token for KG operations

    // === DRAGONFLY INTEGRATION (Bio-inspired Target Tracking and Interception) ===
    //
    // The Dragonfly module provides bio-inspired target tracking and interception:
    // - TSDN: Population vector encoding of target direction (16 neurons, 360°)
    // - CSTMD1: Winner-take-all selective attention for single target lock
    // - Prediction: IMM filter trajectory prediction with evasion detection
    // - Interception: Proportional navigation guidance for optimal pursuit
    //
    // Biological basis: Dragonflies achieve 95% hunting success through:
    // - Internal models predicting prey trajectory
    // - Parallel processing in small target motion detector neurons
    // - Predictive gain modulation along expected target path
    //
    // The dragonfly integrates with:
    // - Visual Cortex Bridge: Target detection from visual processing
    // - Audio Cortex Bridge: Directional cueing from sound localization
    // - Parietal Bridge: Spatial reasoning for interception planning
    // - Cognitive Bridge: Attention allocation and salience detection
    // - Thalamic Bridge: Signal routing and gating
    // - Substrate Bridge: Metabolic costs and fatigue modeling
    // - FEP Bridge: Free energy minimization for prediction
    // - Bio-Async Bridge: Asynchronous neural processing
    // - Global Workspace Bridge: Conscious target awareness
    //
    dragonfly_system_t* dragonfly;                        // Dragonfly target tracking system
    bool dragonfly_enabled;                               // Dragonfly enabled for this brain
    uint64_t last_dragonfly_update_us;                    // Last dragonfly update timestamp
    /* Phase 4i: dragonfly sidecar LNN reservoir (continuous-time smoothing). */
    void* dragonfly_lnn;                                  // dragonfly_lnn_t*
    bool  dragonfly_lnn_enabled;                          // reservoir live flag

    /* Phase 4n-t: JEPA bridges. Each held as void* to keep the bridge
     * headers out of this big include; see nimcp_jepa_brain_bridges.h. */
    void* omni_jepa_bridge;          // omni_jepa_bridge_t*          (4n)
    void* neuromod_jepa_bridge;      // neuromod_jepa_bridge_t*      (4o)
    void* audio_jepa_bridge;         // audio_jepa_bridge_t*         (4q)
    void* engram_jepa_bridge;        // engram_jepa_bridge_t*        (4p)
    void* consolidation_jepa_bridge; // consolidation_jepa_bridge_t* (4t)
    void* cerebellum_jepa_bridge;    // cerebellum_jepa_bridge_t*    (4s)
    void* soma_jepa_bridge;          // soma_jepa_bridge_t*          (4r)

    /* Round A/2: activate 10 formerly-orphaned perception bridges.
     * All void* to keep perception headers out of this TU. Owned by
     * brain — created in brain factory, destroyed in teardown. */
    void* visual_cortex_fep_bridge;    // visual_cortex_fep_bridge_t*
    void* audio_cortex_fep_bridge;     // audio_cortex_fep_bridge_t*
    void* speech_cortex_fep_bridge;    // speech_cortex_fep_bridge_t*
    void* pr_predictive_bridge;        // pr_predictive_bridge_t*
    void* visual_immune_bridge;        // visual_immune_bridge_t*
    void* audio_immune_bridge;         // audio_immune_bridge_t*
    void* speech_immune_bridge;        // speech_immune_bridge_t*
    void* visual_cortical_bridge;      // visual_cortical_bridge_t*
    void* audio_cortical_bridge;       // audio_cortical_bridge_t*
    void* speech_cortical_bridge;      // speech_cortical_bridge_t*

    // Dragonfly-Medulla Integration Bridge
    // BIOLOGICAL: Hunting behavior is modulated by arousal state, circadian rhythm,
    // and protection level. Alert dragonflies hunt better than drowsy ones.
    // - Arousal Level: Affects nav gain, urgency, reaction time
    // - Protection Level: Can block or abort hunting (prioritize survival)
    // - Circadian Phase: Diurnal hunters are inactive at night
    dragonfly_medulla_bridge_t dragonfly_medulla_bridge;  // Dragonfly-medulla integration

    // === ENHANCED BASAL GANGLIA INTEGRATION (Action Selection & Motor Control) ===
    //
    // The Enhanced Basal Ganglia provides biologically-complete action selection:
    // - Core BG: Striatum (D1/D2 MSNs), GPe/GPi, STN, SNc/SNr with DA modulation
    // - Beta Oscillations: 13-30 Hz movement suppression, pathological states
    // - Multi-Neuromodulators: DA, 5HT, ACh, NE, adenosine interactions
    // - Hierarchical RL: Options framework with primitive/hierarchical actions
    // - Model-Based Planning: World model + arbitration with model-free
    // - Nucleus Accumbens: Wanting/liking, Pavlovian-Instrumental Transfer
    // - Superior Colliculus: Saccade generation and orienting responses
    // - Striatal Interneurons: FSI, TAN, LTS timing and modulation
    // - Cerebellar Coordination: Timing and error sharing with cerebellum
    // - Outcome Devaluation: Goal-directed vs habitual behavior testing
    // - Temporal Credit Assignment: TD-lambda eligibility traces
    //
    // The basal ganglia integrates with:
    // - Executive Functions: Goal-directed action selection
    // - Dragonfly: Motor output for pursuit and interception
    // - Medulla: Arousal modulation of action thresholds
    // - Emotional System: Reward/aversion signals to NAc
    // - FEP Orchestrator: Action as free energy minimization
    //
    bg_enhanced_t* basal_ganglia;                         // Enhanced basal ganglia system
    bool basal_ganglia_enabled;                           // Basal ganglia enabled for this brain
    uint64_t last_basal_ganglia_update_us;                // Last BG update timestamp

    // === FAULT TOLERANCE SUBSYSTEM (Intelligent Recovery with Parietal Integration) ===
    //
    // The Fault Tolerance module provides intelligent error recovery through:
    // - Recovery Executive: Multi-step recovery planning and execution
    // - Failure Prediction: Proactive fault detection
    // - Metacognitive Monitoring: "Is this working?" self-assessment
    // - Pattern Learning: Learn from past failures and recoveries
    //
    // BIOLOGICAL BASIS: Maps to brain's error detection and correction:
    // - Anterior Cingulate Cortex: Error monitoring
    // - Prefrontal Cortex: Recovery planning (executive function)
    // - Hippocampus: Episodic memory of past failures
    //
    // PARIETAL INTEGRATION: Uses parietal lobe for:
    // - Software Engineering Analysis: Code structure at failure location
    // - Pattern Detection: Match against historical failures
    // - Spatial Reasoning: Dependency graph analysis
    // - Mathematical Intuition: Recovery feasibility estimation
    //
    struct recovery_executive_internal* recovery_executive; // Recovery planning and execution
    bool fault_tolerance_enabled;                          // Fault tolerance enabled for this brain
    uint64_t last_fault_check_us;                          // Last fault check timestamp

    // === HEALTH AGENT INTEGRATION (Autonomous Health Monitoring) ===
    //
    // The Health Agent provides independent, continuous monitoring of brain health:
    // - Memory: Track allocation trends, detect leaks and corruption
    // - Neural Networks: Monitor SNN/LNN stability, detect divergence
    // - Behavioral: Monitor Dragonfly/Portia behavioral modules
    // - Oscillations: Monitor brain wave patterns for anomalies
    // - Cross-module: Coordinate health across cognitive subsystems
    //
    // DESIGN: The health agent runs in a separate thread for independence.
    // Even if main processing hangs, the agent can still detect and respond.
    //
    struct nimcp_health_agent* health_agent;               // Autonomous health monitoring agent
    bool health_agent_enabled;                             // Health agent enabled for this brain
    bool health_agent_owns_agent;                          // Whether brain owns (should destroy) the agent

    // === BRAIN CYCLE COORDINATOR (Unified Cycle Observability) ===
    //
    // The Cycle Coordinator provides unified observability across all brain cycles:
    // - Timing: Monotonic clock source, stall detection, anomaly tracking
    // - Health: Per-cycle and overall health aggregation (Welford's algorithm)
    // - Dependencies: Declarative cross-cycle dependency graph
    // - Diagnostics: Single entry point for full system health report
    // - Pattern Detection: FNV-1a hash fingerprinting of recurring health states
    //
    // Integrates with:
    // - Immune System: Stall/degradation → antigen presentation
    // - Bio-Async: Health change events published system-wide
    // - KG: Statistics persistence for historical analysis
    // - Introspection, Hemispheric, FEP, Meta-Learning, Pink Noise,
    //   Global Workspace, Attention, World Model (bidirectional callbacks)
    //
    struct brain_cycle_coordinator* cycle_coordinator;     // Brain cycle coordinator
    bool cycle_coordinator_enabled;                        // Cycle coordinator enabled for this brain

    // === STATE MANAGER INTEGRATION (Phase 8: Checkpointing) ===
    //
    // The State Manager provides centralized state checkpointing and recovery:
    // - Checkpoint: Serialize module states for recovery
    // - Restore: Deserialize states after failure
    // - Validate: Verify state integrity
    // - Reset: Return to safe default state
    //
    // Brain subsystems register with state manager for automatic checkpointing.
    //
    struct nimcp_state_manager* state_manager;             // Module state manager
    bool state_manager_enabled;                            // State manager enabled for this brain
    bool state_manager_owns_manager;                       // Whether brain owns (should destroy) manager

    // === BROCA'S REGION INTEGRATION (Language Production) ===
    //
    // Broca's Region (BA44/45) provides language production capabilities:
    // - Syntax Processing: Hierarchical phrase structure generation
    // - Phonological Processing: Sound sequence planning
    // - Speech Motor Planning: Articulatory trajectory generation
    // - Working Memory Integration: Lexical access and retrieval
    //
    // The Broca adapter unifies:
    // - Syntax Processor: Grammatical structure generation
    // - Phonological Processor: Phoneme sequence optimization
    // - Speech Motor Planner: Motor command sequencing
    //
    // Integrates with:
    // - Neural Substrate: Metabolic modulation of speech fluency
    // - Thalamic Router: Motor speech routing through VA/VL nuclei
    // - Quantum Reasoner: Grover-accelerated lexical search
    // - Working Memory: Lexical access buffer
    // - Brain Immune System: Inflammation affects fluency
    // - Training System: Language production learning
    //
    struct broca_adapter* broca;                            // Broca's region adapter
    struct broca_substrate_bridge* broca_substrate_bridge;  // Substrate metabolic integration
    struct broca_thalamic_bridge* broca_thalamic_bridge;    // Thalamic signal routing
    struct broca_quantum_bridge* broca_quantum_bridge;      // Quantum-accelerated language
    bool broca_enabled;                                     // Broca's region enabled for this brain
    uint64_t last_broca_update_us;                          // Last Broca update timestamp

    // === WERNICKE'S REGION INTEGRATION (Language Comprehension) ===
    //
    // Wernicke's Region (posterior STG/BA22) provides language comprehension:
    // - Phonological Analysis: Speech sound recognition (phoneme patterns)
    // - Lexical Access: Word recognition from phoneme sequences (cohort model)
    // - Semantic Integration: Meaning extraction and context integration
    // - Syntactic Comprehension: Sentence structure parsing
    //
    // The Wernicke adapter unifies:
    // - Phonological Analyzer: Phoneme feature extraction
    // - Lexical Access: Word-form to meaning mapping
    // - Semantic Integrator: Concept activation and spreading
    // - Syntactic Parser: Incremental sentence parsing
    //
    // Integrates with:
    // - Neural Substrate: Metabolic modulation of comprehension speed
    // - Quantum Reasoner: Grover-accelerated lexical search
    // - Broca's Area: Arcuate fasciculus connection for production
    // - Semantic Memory: Concept network access
    // - GPU Acceleration: Parallel phoneme/word recognition
    // - Omnidirectional Inference: Predictive language processing
    //
    struct wernicke_adapter* wernicke;                          // Wernicke's region adapter
    struct wernicke_substrate_bridge* wernicke_substrate_bridge; // Substrate metabolic integration
    struct wernicke_quantum_bridge* wernicke_quantum_bridge;    // Quantum-accelerated comprehension
    struct wernicke_broca_bridge* wernicke_broca_bridge;        // Arcuate fasciculus to Broca
    struct omni_wernicke_bridge* omni_wernicke_bridge;          // Omnidirectional inference bridge
    struct wernicke_gpu_bio_bridge* wernicke_gpu_bridge;        // GPU bio-async bridge
    struct wernicke_immune_bridge* wernicke_immune_bridge;      // Wernicke-immune integration
    struct wernicke_nlp_bridge* wernicke_nlp_bridge;            // Comprehensive NLP integration
    bool wernicke_enabled;                                       // Wernicke's region enabled
    uint64_t last_wernicke_update_us;                           // Last Wernicke update timestamp

    // =========================================================================
    // LANGUAGE LAYER (Unified Language Processing Orchestration)
    // =========================================================================
    // The Language Layer unifies all language-related processing:
    // - Wernicke's Area (BA22): Speech comprehension, phonological analysis
    // - Broca's Area (BA44/45): Speech production, syntactic processing
    // - Arcuate Fasciculus: Bidirectional dorsal/ventral streams
    // - NLP Network: Token embeddings, attention, neuromodulation
    // - Speech Cortex: Phoneme extraction, formant analysis
    //
    // Integration Bridges:
    // - Perception Bridge: Speech cortex, audio cortex, visual cortex
    // - Cognitive Bridge: Working memory, attention, semantic memory, reasoning
    // - Training Bridge: Language learning, STDP, vocabulary expansion
    // - Omni Bridge: Predictive language processing (JEPA, Hopfield)
    // - Immune Bridge: Cytokine modulation, aphasia modeling
    // - GPU Bridge: Parallel phoneme/word/semantic processing
    // - Thalamic Bridge: Signal routing through thalamic nuclei
    // - Substrate Bridge: Metabolic modulation (ATP, fatigue, stress)
    // - Logic Bridge: Symbolic reasoning (entailment, consistency)
    //
    struct language_orchestrator* language_layer;                     // Unified language orchestrator
    struct language_perception_bridge* language_perception_bridge;    // Perception integration
    struct language_cognitive_bridge* language_cognitive_bridge;      // Cognitive integration
    struct language_training_bridge* language_training_bridge;        // Training integration
    struct language_omni_bridge* language_omni_bridge;                // Omni inference integration
    struct language_immune_bridge* language_immune_bridge;            // Immune integration
    struct language_gpu_bridge* language_gpu_bridge;                  // GPU acceleration
    struct language_thalamic_bridge* language_thalamic_bridge;        // Thalamic router integration
    struct language_substrate_bridge* language_substrate_bridge;      // Neural substrate integration
    struct language_logic_bridge* language_logic_bridge;              // Symbolic logic integration
    bool language_layer_enabled;                                      // Language layer enabled
    uint64_t last_language_update_us;                                 // Last language update timestamp
    tokenizer_t* tokenizer;                                           // Persistent tokenizer (lazy-init, reused across calls)

    // Language Generator (LNN-based autoregressive decoder)
    struct language_generator* lang_generator;                         // LNN decoder for text generation
    struct embedding_layer* lang_embedding;                            // Token embedding layer for generation

    // Learning workspace (40-watt brain: reuse buffers instead of malloc/free per call)
    struct {
        float* temp_float;              // Reusable float buffer
        uint32_t temp_float_capacity;   // Current capacity
        uint32_t* temp_uint;            // Reusable uint32 buffer
        uint32_t temp_uint_capacity;    // Current capacity
        float* delta_buf;              // Persistent backprop delta buffer
        uint32_t delta_buf_capacity;    // Current capacity
    } learning_workspace;

    // Grounded Language System (human-like word-concept binding)
    struct grounded_language* grounded_lang;                           // Grounded lexicon + production
    struct snn_language_bridge* snn_lang_bridge;                      // SNN spike-driven word-concept binding
    /* PA-3 walkthrough fix: monotonic ms accumulator for the bridge
     * spike-routing path (replaces a thread-unsafe `static float` in
     * brain_tick_language). Per-brain so multiple brains in the same
     * process don't share STDP timing. Bumped by dt_ms each call. */
    float                       lang_bridge_t_ms;
    /* CC-1: tick counter for the periodic bigram-spectrum FFT refresh,
     * bumped each call to brain_tick_language. When it crosses
     * _LANG_SPECTRUM_TICK_DIVISOR (~62 ticks @ 16ms = ~1Hz) the driver
     * calls grounded_language_tick_bigram_spectrum. Per-brain so
     * multi-brain processes don't share phase. */
    uint16_t                    lang_spectrum_tick_counter;
    struct snn_speech_bridge* snn_speech_bridge;                      // SNN spike-driven speech production/comprehension
    struct formant_synth* formant_synth;                              // Formant voice synthesizer (Athena's voice)
    struct lnn_network_s* lnn_prosody;                                // LNN for prosody/F0 contour prediction
    struct snn_audio_bridge* snn_audio_bridge;                        // SNN spike-driven auditory processing
    struct snn_visual_bridge* snn_visual_bridge;                      // SNN spike-driven visual processing
    struct snn_somatosensory_bridge* snn_somatosensory_bridge;        // SNN spike-driven somatosensory processing
    struct cross_modal_align* cross_modal_aligner;                    // Cross-modal temporal alignment

    // Hyperledger-Inspired Training/Inference Integration
    struct hyperledger_bridge* hyperledger_bridge;                     // EOV training + consensus + audit
    bool hyperledger_enabled;                                          // Hyperledger bridge active

    // =========================================================================
    // BRAINSTEM INTEGRATION (Midbrain, Pons, Medulla, Reticular Formation)
    // =========================================================================
    // The Brainstem provides vital functions and reflex control:
    // - Midbrain: Superior/inferior colliculus for visual/auditory orienting
    // - Pons: Relay between cortex and cerebellum, arousal control
    // - Medulla: Vital functions (respiration, heart rate), protective reflexes
    // - Reticular Formation: ARAS arousal control, sleep-wake regulation
    //
    // Integrates with:
    // - Thalamus: Sensory relay to cortex
    // - Cerebellum: Motor coordination via cerebellar peduncles
    // - Spinal Motor: Descending motor pathway control
    // - Medulla: Vital function coordination
    // - Quantum Bridge: Reflex optimization, arousal state optimization
    //
    struct brainstem_adapter* brainstem;                      // Brainstem region adapter
    struct brainstem_substrate_bridge* brainstem_substrate_bridge;  // Substrate metabolic integration
    struct brainstem_thalamic_bridge* brainstem_thalamic_bridge;    // Thalamic signal routing
    struct brainstem_quantum_bridge* brainstem_quantum_bridge;      // Quantum-accelerated processing
    bool brainstem_enabled;                                   // Brainstem enabled for this brain
    uint64_t last_brainstem_update_us;                        // Last brainstem update timestamp

    // =========================================================================
    // CEREBELLUM INTEGRATION (Motor Coordination & Timing)
    // =========================================================================
    // The Cerebellum provides motor coordination using Marr-Albus-Ito model:
    // - Granule cells: Sparse coding of mossy fiber inputs (~50B cells)
    // - Purkinje cells: Integrate parallel fibers, inhibit deep nuclei
    // - Deep nuclei: Dentate (planning), Interposed (execution), Fastigial (balance)
    // - Climbing fibers: Error signals from inferior olive trigger LTD
    //
    // Integrates with:
    // - Motor Cortex: Motor command execution
    // - Basal Ganglia: Action selection coordination
    // - Brainstem: Postural control and balance
    // - Thalamic Router: VL nucleus for cortical relay
    // - Quantum Reasoner: Grover-accelerated timing optimization
    //
    struct cerebellum_adapter* cerebellum;                      // Cerebellum adapter
    struct cerebellum_substrate_bridge* cerebellum_substrate_bridge;  // Substrate metabolic integration
    struct cerebellum_thalamic_bridge* cerebellum_thalamic_bridge;    // Thalamic signal routing
    struct cerebellum_quantum_bridge* cerebellum_quantum_bridge;      // Quantum-accelerated timing
    bool cerebellum_enabled;                                    // Cerebellum enabled for this brain
    uint64_t last_cerebellum_update_us;                         // Last Cerebellum update timestamp

    // =========================================================================
    // HIPPOCAMPUS INTEGRATION (Episodic Memory & Spatial Navigation)
    // =========================================================================
    // The Hippocampus provides memory encoding, consolidation, and spatial navigation:
    // - CA1/CA3: Pyramidal cells for memory encoding/retrieval
    // - Dentate Gyrus: Pattern separation (sparse coding, reduce interference)
    // - Entorhinal Cortex: Grid cells for path integration
    // - Place Cells: Location-specific firing for cognitive spatial map
    // - Pattern Completion: Reconstruct full patterns from partial cues (CA3)
    //
    // Integrates with:
    // - Neural Substrate: Metabolic modulation of consolidation
    // - Thalamic Router: Anterior nucleus routing of memory signals
    // - Quantum Reasoner: Grover-accelerated memory search (O(sqrt(N)))
    // - Cortical Areas: Systems consolidation to neocortex
    // - Amygdala: Emotional memory tagging
    // - Sleep System: Memory consolidation during sleep stages
    // - Training System: Experience-dependent learning
    //
    struct hippocampus_adapter* hippocampus;                        // Hippocampus adapter
    struct hippocampus_substrate_bridge* hippocampus_substrate_bridge;  // Substrate metabolic integration
    struct hippocampus_thalamic_bridge* hippocampus_thalamic_bridge;    // Thalamic signal routing
    struct hippocampus_quantum_bridge* hippocampus_quantum_bridge;      // Quantum-accelerated memory
    bool hippocampus_enabled;                                       // Hippocampus enabled for this brain
    uint64_t last_hippocampus_update_us;                            // Last hippocampus update timestamp

    // =========================================================================
    // HYPOTHALAMUS INTEGRATION (Homeostatic Regulation & Autonomic Control)
    // =========================================================================
    // The Hypothalamus regulates homeostasis, circadian rhythms, and autonomic functions:
    // - SCN: Suprachiasmatic nucleus - master circadian clock
    // - Thermoregulation: Body temperature control centers
    // - Appetite/Satiety: Feeding behavior and energy balance
    // - HPA Axis: Stress response (CRH -> ACTH -> cortisol)
    // - Sleep-wake: Orexin neurons for arousal, VLPO for sleep
    //
    // Integrates with:
    // - Medulla: Autonomic reflex control
    // - Pituitary: Hormonal release coordination
    // - Brainstem: Arousal and sleep regulation
    // - Immune System: Neuroimmune interactions
    // - Wellbeing Module: Affective state modulation
    //
    struct hypothalamus_adapter* hypothalamus;                        // Hypothalamus adapter
    struct hypothalamus_substrate_bridge* hypothalamus_substrate_bridge;  // Substrate metabolic integration
    struct hypothalamus_thalamic_bridge* hypothalamus_thalamic_bridge;    // Thalamic signal routing
    struct hypothalamus_quantum_bridge* hypothalamus_quantum_bridge;      // Quantum-accelerated optimization
    bool hypothalamus_enabled;                                        // Hypothalamus enabled for this brain
    uint64_t last_hypothalamus_update_us;                             // Last hypothalamus update timestamp

    // Entorhinal adapter (Wave 8B-c tick target). Owned at brain level
    // so the dt-driven tick can advance grid-cell / spatial state.
    struct entorhinal_adapter* entorhinal;                             // Entorhinal adapter
    bool entorhinal_enabled;                                           // Enabled flag

    // =========================================================================
    // MOTOR CORTEX INTEGRATION (Primary Motor Control - M1)
    // =========================================================================
    // The Motor Cortex provides voluntary movement control:
    // - Primary Motor Cortex (M1): Movement execution, somatotopic organization
    // - Premotor Cortex (PMC): Movement planning and preparation
    // - Supplementary Motor Area (SMA): Complex movement sequencing
    // - Motor Homunculus: Body part representation in motor strip
    //
    // Integrates with:
    // - Basal Ganglia: Action selection coordination
    // - Cerebellum: Motor timing and error correction
    // - Thalamus: VA/VL nucleus motor relay
    // - Prefrontal Cortex: Goal-directed movement planning
    // - Spinal Motor Neurons: Final motor output pathway
    //
    struct motor_adapter* motor;                                      // Motor cortex adapter
    struct motor_substrate_bridge* motor_substrate_bridge;            // Substrate metabolic integration
    struct motor_thalamic_bridge* motor_thalamic_bridge;              // Thalamic signal routing
    struct motor_quantum_bridge* motor_quantum_bridge;                // Quantum-accelerated optimization
    bool motor_enabled;                                               // Motor cortex enabled for this brain
    uint64_t last_motor_update_us;                                    // Last motor update timestamp

    // =========================================================================
    // OCCIPITAL CORTEX INTEGRATION (Visual Processing - V1-V5)
    // =========================================================================
    // The Occipital Cortex provides visual processing:
    // - V1 (Primary Visual): Edge detection, orientation selectivity
    // - V2: Texture, color, illusory contours
    // - V3/V3A: Form processing, dynamic shape
    // - V4: Color constancy, object recognition
    // - V5/MT: Motion detection, optic flow
    //
    // Integrates with:
    // - Temporal Cortex: Object recognition (ventral stream)
    // - Parietal Cortex: Spatial processing (dorsal stream)
    // - Superior Colliculus: Eye movement coordination
    // - LGN (Thalamus): Visual input relay
    // - Dragonfly: Target tracking for pursuit
    //
    struct occipital_adapter* occipital;                              // Occipital cortex adapter
    struct occipital_substrate_bridge* occipital_substrate_bridge;    // Substrate metabolic integration
    struct occipital_thalamic_bridge* occipital_thalamic_bridge;      // Thalamic signal routing
    struct occipital_quantum_bridge* occipital_quantum_bridge;        // Quantum-accelerated vision
    bool occipital_enabled;                                           // Occipital cortex enabled for this brain
    uint64_t last_occipital_update_us;                                // Last occipital update timestamp

    // =========================================================================
    // TEMPORAL CORTEX INTEGRATION (Auditory & Object Recognition)
    // =========================================================================
    // The Temporal Cortex provides auditory and object processing:
    // - A1 (Primary Auditory): Tonotopic sound processing
    // - Superior Temporal Gyrus (STG): Speech comprehension (Wernicke's)
    // - Middle Temporal Gyrus (MTG): Semantic processing
    // - Inferior Temporal (IT): Object recognition, face processing
    // - Fusiform Face Area (FFA): Face-specific processing
    //
    // Integrates with:
    // - Hippocampus: Memory encoding of objects/events
    // - Amygdala: Emotional valence of sounds/faces
    // - Prefrontal Cortex: Working memory for auditory/visual info
    // - Occipital Cortex: Visual object information (ventral stream)
    // - Broca's Area: Language production connection
    //
    struct temporal_adapter* temporal;                                // Temporal cortex adapter
    struct temporal_substrate_bridge* temporal_substrate_bridge;      // Substrate metabolic integration
    struct temporal_thalamic_bridge* temporal_thalamic_bridge;        // Thalamic signal routing
    struct temporal_quantum_bridge* temporal_quantum_bridge;          // Quantum-accelerated recognition
    bool temporal_enabled;                                            // Temporal cortex enabled for this brain
    uint64_t last_temporal_update_us;                                 // Last temporal update timestamp

    // =========================================================================
    // PREFRONTAL CORTEX INTEGRATION (Executive Functions - PFC)
    // =========================================================================
    // The Prefrontal Cortex provides executive control:
    // - DLPFC (Dorsolateral): Working memory, cognitive control
    // - VLPFC (Ventrolateral): Response inhibition, rule learning
    // - OFC (Orbitofrontal): Value-based decision making
    // - mPFC (Medial): Self-referential processing, social cognition
    // - ACC Integration: Error monitoring and conflict resolution
    //
    // Integrates with:
    // - Basal Ganglia: Goal-directed action selection
    // - Working Memory: Active maintenance and manipulation
    // - Thalamus: MD nucleus bidirectional relay
    // - Hippocampus: Episodic memory retrieval
    // - Amygdala: Emotional regulation
    //
    struct prefrontal_adapter* prefrontal;                            // Prefrontal cortex adapter
    struct prefrontal_substrate_bridge* prefrontal_substrate_bridge;  // Substrate metabolic integration
    struct prefrontal_thalamic_bridge* prefrontal_thalamic_bridge;    // Thalamic signal routing
    struct prefrontal_quantum_bridge* prefrontal_quantum_bridge;      // Quantum-accelerated planning
    bool prefrontal_enabled;                                          // Prefrontal cortex enabled for this brain
    uint64_t last_prefrontal_update_us;                               // Last prefrontal update timestamp

    // =========================================================================
    // INSULA INTEGRATION (Interoception & Emotional Awareness)
    // =========================================================================
    // The Insula provides interoceptive and emotional awareness:
    // - Anterior Insula: Subjective feelings, empathy, awareness
    // - Posterior Insula: Somatosensory integration, pain processing
    // - Interoception: Internal body state monitoring (heart, gut)
    // - Disgust Processing: Both physical and moral disgust
    // - Addiction/Craving: Drug seeking and withdrawal states
    //
    // Integrates with:
    // - ACC: Emotional salience and motivation
    // - Amygdala: Emotional processing coordination
    // - Hypothalamus: Autonomic regulation
    // - Somatosensory Cortex: Body state integration
    // - Social Cognition: Empathy and social emotions
    //
    struct insula_adapter* insula;                                    // Insula adapter
    struct insula_quantum_bridge* insula_quantum_bridge;              // Quantum-accelerated interoception
    bool insula_enabled;                                              // Insula enabled for this brain
    uint64_t last_insula_update_us;                                   // Last insula update timestamp

    // =========================================================================
    // CINGULATE CORTEX INTEGRATION (Error Monitoring & Cognitive Control)
    // =========================================================================
    // The Cingulate Cortex provides error monitoring and control:
    // - ACC (Anterior Cingulate): Error detection, conflict monitoring
    // - MCC (Midcingulate): Motor intention, pain processing
    // - PCC (Posterior Cingulate): Autobiographical memory, DMN hub
    // - Error-Related Negativity (ERN): Real-time error signals
    // - Conflict Adaptation: Adjusting control after errors
    //
    // Integrates with:
    // - Prefrontal Cortex: Executive control coordination
    // - Insula: Emotional salience integration
    // - Motor Cortex: Action monitoring
    // - Emotional System: Affective state influence
    // - Autobiographical Memory: Self-referential processing
    //
    struct cingulate_adapter* cingulate;                              // Cingulate cortex adapter
    struct cingulate_quantum_bridge* cingulate_quantum_bridge;        // Quantum-accelerated monitoring
    bool cingulate_enabled;                                           // Cingulate enabled for this brain
    uint64_t last_cingulate_update_us;                                // Last cingulate update timestamp

    // =========================================================================
    // ORBITOFRONTAL CORTEX (OFC) — Reward valuation + reversal learning
    // =========================================================================
    // OFC binds stimulus values to reward-prediction errors. Used by the
    // grounded_language regional bridge to tag groundings with a value
    // estimate (gut-feel for whether an utterance is rewarding).
    struct nimcp_ofc* ofc;                                            // OFC adapter
    bool ofc_enabled;                                                 // OFC enabled for this brain

    // =========================================================================
    // PARIETAL CORTEX INTEGRATION (Spatial Processing & Attention)
    // =========================================================================
    // The Parietal Cortex provides spatial processing and attention:
    // - SPL (Superior Parietal Lobule): Spatial attention, reaching
    // - IPL (Inferior Parietal Lobule): Semantic integration, tool use
    // - IPS (Intraparietal Sulcus): Number sense, eye movements
    // - Precuneus: Spatial imagery, episodic memory
    // - Angular Gyrus: Language, semantic memory
    //
    // NOTE: This is distinct from cognitive/parietal which handles
    // mathematical and scientific reasoning. This is the sensorimotor
    // parietal cortex for spatial attention and body awareness.
    //
    // Integrates with:
    // - Motor Cortex: Reaching and grasping coordination
    // - Occipital Cortex: Dorsal visual stream (where pathway)
    // - Prefrontal Cortex: Attention control
    // - Cerebellum: Spatial coordination
    // - Dragonfly: Target spatial tracking
    //
    struct parietal_adapter* parietal_cortex;                  // Parietal cortex adapter
    struct parietal_cortex_substrate_bridge* parietal_cortex_substrate_bridge;  // Substrate integration
    struct parietal_cortex_thalamic_bridge* parietal_cortex_thalamic_bridge;    // Thalamic routing
    struct parietal_quantum_bridge* parietal_cortex_quantum_bridge;      // Quantum-accelerated spatial
    bool parietal_cortex_enabled;                                     // Parietal cortex enabled for this brain
    uint64_t last_parietal_cortex_update_us;                          // Last parietal cortex update timestamp

    // =========================================================================
    // PHASE 6 SENSORY MODULES (Somatosensory, Olfactory, Gustatory)
    // =========================================================================
    // These modules implement the three remaining sensory modalities:
    //
    // SOMATOSENSORY CORTEX (BR-9): Touch, proprioception, temperature, pain
    // - Area 3a: Proprioception (muscle spindles, joint receptors)
    // - Area 3b: Fine touch (Meissner, Merkel receptors)
    // - Area 1: Texture processing
    // - Area 2: Size/shape integration
    // - S2: Secondary somatosensory (bilateral integration)
    // - Somatotopic body map (homunculus)
    //
    // Integrates with:
    // - Thalamus VPL/VPM nuclei for relay
    // - Motor cortex for sensorimotor integration
    // - Parietal cortex for body awareness
    // - Hypothalamus for temperature regulation
    // - Pain pathway (spinothalamic)
    //
    struct nimcp_somatosensory_s* somatosensory;                      // Somatosensory cortex (S1/S2)
    struct soma_substrate_bridge* somatosensory_substrate_bridge;     // Substrate metabolic integration
    struct soma_thalamic_bridge* somatosensory_thalamic_bridge;       // Thalamic VPL/VPM routing
    bool somatosensory_enabled;                                       // Somatosensory enabled for this brain
    uint64_t last_somatosensory_update_us;                            // Last somatosensory update timestamp

    // =========================================================================
    // OLFACTORY CORTEX (BR-10): Smell processing
    // - Olfactory bulb (glomeruli, mitral cells)
    // - Piriform cortex (primary olfactory)
    // - Orbitofrontal cortex (odor identity)
    // - ~400 odorant receptor types
    // - Combinatorial odor coding
    // - Direct cortical access (bypasses thalamus)
    //
    // Integrates with:
    // - Amygdala: Emotional associations (Proustian memory)
    // - Hippocampus: Episodic memory encoding
    // - Gustatory cortex: Flavor perception
    // - Hypothalamus: Food seeking, pheromones
    //
    struct nimcp_olfactory_s* olfactory;                              // Olfactory/piriform cortex
    struct olfact_substrate_bridge* olfactory_substrate_bridge;       // Substrate metabolic integration
    bool olfactory_enabled;                                           // Olfactory enabled for this brain
    uint64_t last_olfactory_update_us;                                // Last olfactory update timestamp

    // =========================================================================
    // GUSTATORY CORTEX (BR-11): Taste processing
    // - Five basic tastes: sweet, salty, sour, bitter, umami
    // - Insular cortex (primary gustatory)
    // - Orbitofrontal cortex (flavor identity)
    // - Taste-reward integration
    // - Disgust response pathway
    //
    // Integrates with:
    // - Olfactory cortex: Flavor perception
    // - Hypothalamus: Appetite, satiety
    // - Amygdala: Food preferences
    // - Nucleus accumbens: Reward processing
    // - Brainstem: Gag reflex, salivation
    //
    struct nimcp_gustatory_s* gustatory;                              // Gustatory/insular cortex
    struct gust_substrate_bridge* gustatory_substrate_bridge;         // Substrate metabolic integration
    bool gustatory_enabled;                                           // Gustatory enabled for this brain
    uint64_t last_gustatory_update_us;                                // Last gustatory update timestamp

    // =========================================================================
    // GPU CONTEXT INTEGRATION (CUDA Kernel Acceleration)
    // =========================================================================
    // The GPU Context provides unified GPU resource management for CUDA kernels:
    // - Device Management: CUDA device selection and configuration
    // - Stream Management: Compute and transfer streams for async ops
    // - Library Handles: cuBLAS, cuFFT for accelerated operations
    // - Memory Tracking: GPU allocation/deallocation tracking
    //
    // Integrates with:
    // - Training: GPU-accelerated gradient computation and optimizer steps
    // - Inference: Fast forward pass with fused kernels
    // - SNN: GPU neuron simulation (LIF, Izhikevich)
    // - CNN: Convolution, pooling for audio/visual/speech processing
    // - LNN: ODE solvers for Liquid Neural Networks
    // - Tensor Ops: GEMM, element-wise, reductions, FFT
    // - Quantum: GPU-accelerated quantum algorithm simulation
    //
    // Auto-initialized during brain creation if GPU is available.
    // Falls back to CPU execution if no GPU or CUDA unavailable.
    //
    struct nimcp_gpu_context_s* gpu_ctx;                              // GPU context for CUDA acceleration
    bool gpu_enabled;                                                 // GPU acceleration enabled for this brain
    uint64_t last_gpu_sync_us;                                        // Last GPU synchronization timestamp
    struct gpu_plasticity_state_s* gpu_plasticity_state;             // GPU plasticity state (STDP/BCM/homeostatic)

    //=========================================================================
    // GPU Neural Substrate Context (Unified GPU Substrate Processing)
    //=========================================================================
    //
    // Unified GPU acceleration for all neural substrate components:
    // - Axons: Signal propagation, refractory dynamics, myelination effects
    // - Dendrites: Cable equation, NMDA spikes, calcium dynamics, bAP
    // - Myelin: G-ratio, conduction velocity, activity-dependent plasticity
    // - Neuromodulators: DA/5HT/ACh/NE dynamics, phasic-tonic, receptor effects
    // - Glial: Astrocyte calcium waves, microglia activation, OPC differentiation
    // - Metabolic: ATP/oxygen/glucose constraints, fatigue modeling
    //
    // Auto-initialized during brain creation if GPU is available.
    // Provides tensor-based batch operations for large-scale simulations.
    //
    struct substrate_gpu_context* substrate_gpu_ctx;                  // Unified substrate GPU context

    //=========================================================================
    // NEURAL SUBSTRATE + THALAMIC ROUTER (CPU-side biological adapters)
    //=========================================================================
    //
    // Phase 1-4 adapter substrate: attached to SNN/LNN/CNN networks via
    // *_attach_substrate and *_attach_thalamic_router calls. Substrate
    // tracks ATP/O2/temperature/ion/membrane state and modulates firing,
    // tau, plasticity. Thalamic router gates signal routing between the
    // network outputs and downstream targets using attention weights.
    //
    // These are borrowed-pointer owners: lifecycle begins in the brain
    // init wave (substrate_thalamic_adapters) and ends in brain_destroy.
    // Networks keep borrowed pointers — they do NOT destroy these.
    //
    // NULL-tolerant: if creation fails, attach calls are no-ops and
    // networks fall back to biological defaults.
    //
    struct neural_substrate* substrate;                               // CPU neural substrate (metabolic state)
    struct thalamic_router* thalamic_router;                          // Attention-gated signal router
    bool substrate_enabled;                                           // True if substrate successfully created
    bool thalamic_router_enabled;                                     // True if router successfully created

    //=========================================================================
    // SLEEP-WAKE BRIDGES (substrate, thalamic)
    //=========================================================================
    // Sleep-wake adapters that couple the biological substrate + thalamic
    // gating to the sleep cycle. Both are NULL-tolerant: if substrate or
    // thalamic_router doesn't exist, or if bridge creation fails, the
    // pointer stays NULL and sleep cycles degrade gracefully.
    //
    // substrate bridge: reads real substrate ATP/fatigue each sleep tick,
    // derives arousal/sleep_pressure/circadian_phase, pushes via bio-async.
    // Wires metabolic fatigue into sleep pressure directly.
    //
    // thalamic bridge: gates the thalamic router's attention output by
    // arousal level — during deep NREM, attention dampens; during REM and
    // waking, attention amplifies. Exposes sleep state as a modulator.
    //
    struct sleep_wake_substrate_bridge* sleep_wake_substrate_bridge;  // Sleep↔substrate ATP coupling
    struct sleep_wake_thalamic_bridge* sleep_wake_thalamic_bridge;    // Sleep↔thalamic attention gating

    //=========================================================================
    // BIOLOGY MODULES — Wired 2026-04-24 via cycle coordinator register_driven
    //=========================================================================
    // Three biology modules previously existed as statues (create/update APIs
    // present, but never instantiated and never called). Now created at brain
    // init and driven periodically by the cycle coordinator:
    //
    //   - epigenetics (100ms): methylation / histone / chromatin state per
    //     neuron. Outputs get_plasticity(neuron_id) → multiplier for STDP.
    //     Critical-period gating for developmental windows.
    //
    //   - neurogenesis (1s): adult neurogenesis cycle. Creates new neurons,
    //     matures young ones, prunes unused. Callbacks fire on lifecycle
    //     events.
    //
    //   - neurovascular (100ms): neurovascular coupling. Neural activity →
    //     CBF change → HRF → BOLD signal. Downstream readers can use the
    //     BOLD trace as a biologically plausible activity integrator.
    //
    // All three are NULL-tolerant: if create fails, the field stays NULL and
    // the driver just isn't registered. Consumer-side wiring (reading
    // get_plasticity() in STDP, listening to neurogenesis callbacks in the
    // SNN population manager, sampling BOLD in metabolic modulation) is a
    // separate follow-up — the present change only advances the state.
    //
    nimcp_epigenetics_t epigenetics;                                  // Methylation / histone / chromatin state
    nimcp_neurogenesis_t neurogenesis;                                // Adult neuron birth / maturation / pruning
    nimcp_nvc_system_t* nvc_system;                                   // Neurovascular coupling — HRF + BOLD
    bool epigenetics_enabled;                                         // True if create + register_driven succeeded
    bool neurogenesis_enabled;
    bool neurovascular_enabled;

    //=========================================================================
    // CHEMISTRY STACK — Wired 2026-04-24 via cycle coordinator register_driven
    //=========================================================================
    // Four chemistry modules previously statues (compile-disabled — their .c
    // files were not in any CMakeLists.txt). Wave 6 added them to the build
    // and wires a single BRAIN_CYCLE_CHEMISTRY cycle at 10ms that updates
    // all four in biological order: protons → buffers → pH → NO.
    //
    // Forward-declared as opaque structs to keep the chemistry headers out
    // of brain_internal.h — each of those headers redefines nimcp_brain_t
    // and would collide with the public nimcp.h typedef.
    //
    struct nimcp_ph_system_s* ph_system;                              // Intracellular/extracellular pH
    struct nimcp_pump_system_s* proton_pumps;                         // V-ATPase, NHE, NBC pumps
    struct nimcp_buffer_manager_s* buffer_manager;                    // Bicarbonate, phosphate, protein buffers
    struct nimcp_no_system_s* no_system;                              // Nitric oxide gasotransmitter
    bool chemistry_enabled;                                           // True if all 4 modules + register_driven succeeded

    //=========================================================================
    // PREDICTIVE-IMMUNE COUPLING — Wired 2026-04-24 via cycle coordinator
    //=========================================================================
    // Integration layer between predictive processing and brain immune system.
    // Previously a statue — create/update APIs implemented but never called.
    //
    // Bidirectional coupling:
    //   - Immune → Predictive: inflammation reduces prediction precision
    //     weights (cytokine noise model).
    //   - Predictive → Immune: large/persistent prediction errors trigger
    //     immune antigen presentation (adversarial input detector).
    //
    // Depends on: brain->predictive_network (Wave 8) + brain->immune_system
    // (Wave 11). Init lives in Wave 12; if either prereq is NULL the init is
    // a no-op and `predictive_immune` stays NULL (module stays a statue).
    //
    predictive_immune_system_t* predictive_immune;                    // Predictive ↔ immune coupling system (opaque)
    bool predictive_immune_enabled;                                   // True if create + register_driven succeeded

    //=========================================================================
    // COCHLEA + COCHLEA BRIDGES — Wired 2026-04-24 (Wave 8A)
    //=========================================================================
    // The cochlea subsystem + its 15 consumer bridges were flagged as
    // HALF-STATUEs in the consumer-bridge inventory: .c files compile but
    // zero callers from brain_decide / brain_learn_vector. Wave 8A creates
    // brain->cochlea at init (after immune+fep are up), creates each of the
    // 15 bridges with their respective deps, registers BRAIN_CYCLE_COCHLEA_
    // BRIDGES at 10ms to tick them in sequence, and wires update calls into
    // both hot paths (brain_decide + brain_learn_vector).
    //
    // Fields are null-tolerant: if any bridge's dep (e.g. immune_system,
    // fep_orchestrator) is NULL at init time, that bridge stays NULL and
    // the update call no-ops. Same semantics as biology cluster.
    //
    struct cochlea*                         cochlea;                      // Cochlea pipeline (BM → HC → ANF)
    bool                                    cochlea_enabled;              // True if create succeeded

    struct cochlea_audio_cortex_bridge*     cochlea_audio_cortex_bridge;  // Cochlea → A1 auditory cortex
    struct cochlea_bio_async_bridge*        cochlea_bio_async_bridge;     // Cochlea → bio-async router
    struct cochlea_broca_bridge*            cochlea_broca_bridge;         // Cochlea → Broca speech
    struct cochlea_collective_bridge*       cochlea_collective_bridge;    // Cochlea → collective cognition
    struct cochlea_cortical_deep_bridge*    cochlea_cortical_deep_bridge; // Cochlea → deep cortical columns
    struct cochlea_fep_bridge*              cochlea_fep_bridge;           // Cochlea → FEP orchestrator
    struct cochlea_immune_bridge*           cochlea_immune_bridge;        // Cochlea → brain immune system
    struct cochlea_kg_bridge*                cochlea_kg_bridge;            // Cochlea → knowledge graph
    struct cochlea_medulla_bridge*          cochlea_medulla_bridge;       // Cochlea → medulla brainstem
    struct cochlea_occipital_bridge*        cochlea_occipital_bridge;     // Cochlea → occipital audiovisual
    struct cochlea_rcog_bridge*             cochlea_rcog_bridge;          // Cochlea → recursive cognition
    struct cochlea_sleep_bridge*            cochlea_sleep_bridge;         // Cochlea → sleep controller
    struct cochlea_substrate_bridge*        cochlea_substrate_bridge;     // Cochlea → neural substrate
    struct cochlea_thalamic_bridge*         cochlea_thalamic_bridge;      // Cochlea → thalamus MGN
    struct cochlea_verification_bridge*     cochlea_verification_bridge;  // Cochlea self-verification
    bool                                    cochlea_bridges_enabled;      // True if register_driven succeeded
    void*                                   cochlea_output_buffer;        // Wave 8A+ (2026-04-24): cochlea_output_t* — shared zero-fill output buffer passed to all 11 NULL-rejecting bridges; owned by brain, created/destroyed with cochlea. Void* because cochlea_output_t is an anonymous-struct typedef.

    //=========================================================================
    // Physics Bridges (Wave 8C, 2026-04-24)
    //
    // Wire the 4 HALF-STATUE physics bridges that have create/destroy/update:
    //   - ephaptic_bio_async  (Ephaptic → bio-router)
    //   - ephaptic_fft        (Ephaptic LFP → FFT spectral analysis)
    //   - hh_bio_async        (Hodgkin-Huxley → bio-router)
    //   - thermo_bio_async    (Thermodynamics → bio-router)
    //
    // The 3 quantum bridges (ephaptic_quantum, hh_quantum, thermo_quantum)
    // are purely functional (no create/destroy/update) — they are called
    // on-demand by other subsystems, not ticked from a hot path. Skipped
    // from this wiring.
    //
    // Upstream physics modules stored as void* to sidestep typedef collisions
    // (see forward-decl block above). Cast at the init/tick call sites.
    //=========================================================================
    void*                                   ephaptic_system;              // nimcp_ephaptic_system_t* (heap)
    void*                                   thermo_state;                 // nimcp_thermodynamic_state_t* (heap)
    void*                                   hh_population;                // nimcp_hh_population_t* (heap)

    struct ephaptic_bio_async_bridge_struct* ephaptic_bio_async_bridge;   // Ephaptic → bio-router
    struct ephaptic_fft_bridge_struct*       ephaptic_fft_bridge;         // Ephaptic LFP → FFT spectral
    struct hh_bio_async_bridge_struct*       hh_bio_async_bridge;         // HH → bio-router
    struct thermo_bio_async_bridge_struct*   thermo_bio_async_bridge;     // Thermo → bio-router

    bool                                    physics_bridges_enabled;      // True if any physics bridge is live

    //=========================================================================
    // GPU Inference Optimization (Phase GPU-INF)
    //=========================================================================
    nimcp_thread_pool_t* inference_pool;                              // Thread pool for parallel cognitive stages
    bool frozen;                                                      // Frozen inference mode — no learning

    // =========================================================================
    // PRIME RESONANT MEMORY SYSTEM (Content-Addressable Consolidation)
    // =========================================================================
    // The Prime Resonant (PR) Memory System provides biologically-inspired
    // memory consolidation and retrieval:
    // - Z-Ladder: Four-tier consolidation (Z0-working, Z1-short, Z2-long, Z3-permanent)
    // - Theta-Gamma: Phase-gated encoding (0-90°) and retrieval (180-270°) windows
    // - Entanglement: Associative memory graph with resonance-weighted edges
    // - Prime Signatures: Content-addressable indexing via prime factorization
    // - Quaternion State: 4D encoding of consolidation/emotion/salience/accessibility
    //
    // Integrates with:
    // - Hippocampus: Z-Ladder consolidation maps to hippocampal memory stages
    // - Working Memory: Z0 tier corresponds to prefrontal working memory buffer
    // - Sleep System: Consolidation accelerated during sleep stages
    // - Emotional System: Salience affects promotion/decay rates
    // - Training System: PR bridges connect to plasticity mechanisms
    // - Perception: Visual/audio/speech bridges encode sensory memories
    //
    struct z_ladder_struct* pr_z_ladder;                              // Z-Ladder memory tier manager
    struct theta_gamma_manager_internal* pr_theta_gamma;              // Theta-gamma phase coupling
    struct entangle_graph_struct* pr_entanglement;                    // Memory entanglement graph
    /* Phase E2: node manager for allocating pr_memory_node_t instances
     * that get inserted into pr_z_ladder. Kept as void* to avoid pulling
     * the pr_memory_node header into this TU. */
    void* pr_node_manager;                                            // pr_node_manager_t

    /* Phase E3: auto-insertion heuristics. When enabled, brain_learn_vector
     * auto-inserts the training features as a memory node when the caller's
     * confidence exceeds a threshold; if confidence crosses a higher landmark
     * threshold, the node is also marked as a landmark (promoted to Z3,
     * protected from demotion). */
    bool     pr_auto_insert_enabled;       // default off — opt in via Python
    float    pr_auto_insert_confidence;    // min confidence to auto-insert (0.7f default)
    float    pr_auto_landmark_confidence;  // min confidence to also mark landmark (0.95f default)
    uint64_t pr_auto_insert_count;         // observable counter
    uint64_t pr_auto_landmark_count;       // observable counter
    bool pr_memory_enabled;                                           // PR memory system enabled
    bool pr_lazy_init;                                                // Defer PR memory initialization
    uint64_t last_pr_consolidation_us;                                // Last consolidation timestamp
    uint64_t pr_consolidation_interval_us;                            // Consolidation interval (default: 100ms)
    uint64_t pr_consolidation_count;                                  // Phase E4/E5: consolidation tick counter for periodic landmark hygiene

    // Phase E6: autonomous pr_memory driver thread — runs pr_memory_tick
    // + notifies BRAIN_CYCLE_LONG_TERM_MEMORY at 100ms cadence even when
    // brain_learn_vector isn't firing (daemon idle, pure inference).
    void*    pr_memory_driver_thread;   // nimcp_thread_t* (heap-allocated)
    int      pr_memory_driver_stop;     // atomic-style stop flag (0=run, 1=stop)
    uint64_t pr_memory_driver_ticks;    // observable counter
    void*    pr_memory_unified_mm;      // unified_mem_manager_t for pr_node_manager (Phase E6)
    void*    pr_memory_tick_mutex;      // nimcp_mutex_t* — serializes pr_memory_tick callers (learn_vector + driver thread)

    // =========================================================================
    // PHASE 4 NEUROMODULATORY NUCLEI INTEGRATION (LC, VTA, Raphe, Habenula)
    // =========================================================================
    // The neuromodulatory nuclei provide modulatory control over brain activity:
    // - Locus Coeruleus (LC): Norepinephrine (NE) - arousal, attention, stress
    // - Ventral Tegmental Area (VTA): Dopamine (DA) - reward, motivation, learning
    // - Raphe Nuclei: Serotonin (5-HT) - mood, impulse control, patience
    // - Habenula: Aversion - disappointment, negative outcomes, avoidance
    //
    // These nuclei have widespread projections to cortical and subcortical areas
    // and coordinate to produce coherent neuromodulatory states.
    //
    // BIOLOGICAL BASIS:
    // - LC: ~1500 neurons/hemisphere with 300,000+ projections each
    // - VTA: Dopaminergic neurons projecting to NAc, PFC, amygdala
    // - Raphe: Largest serotonergic source, mood/circadian regulation
    // - Habenula: "Anti-reward" center, modulates VTA and raphe
    //
    // Integrates with:
    // - Security System: Neuromodulator-mediated threat response
    // - Immune System: Psychoneuroimmunology (cytokine-neuromodulator crosstalk)
    // - Logging System: Audit trails for neuromodulatory events
    // - Training System: Bidirectional learning rate modulation
    // - Bio-Async: Cross-nuclei coordination messaging
    //
    struct nimcp_lc_adapter_struct* lc_adapter;                       // Locus Coeruleus adapter
    struct nimcp_vta_adapter* vta_adapter;                            // VTA adapter
    struct nimcp_raphe_adapter* raphe_adapter;                        // Raphe nuclei adapter
    struct nimcp_habenula_adapter_impl* habenula_adapter;             // Habenula adapter
    struct nimcp_neuromod_intra_struct* neuromod_intra_coordinator;   // Neuromodulatory intra-layer coordinator
    bool lc_enabled;                                                  // LC enabled for this brain
    bool vta_enabled;                                                 // VTA enabled for this brain
    bool raphe_enabled;                                               // Raphe enabled for this brain
    bool habenula_enabled;                                            // Habenula enabled for this brain
    bool neuromod_intra_enabled;                                      // Neuromod intra-coordinator enabled
    uint64_t last_lc_update_us;                                       // Last LC update timestamp
    uint64_t last_vta_update_us;                                      // Last VTA update timestamp
    uint64_t last_raphe_update_us;                                    // Last Raphe update timestamp
    uint64_t last_habenula_update_us;                                 // Last Habenula update timestamp

    // =========================================================================
    // WORLD MODEL INTEGRATION (Generative World Model for Mental Simulation)
    // =========================================================================
    // The World Model provides generative simulation capabilities:
    // - Omni World Model: Omnidirectional prediction (forward/backward/lateral)
    // - Multimodal World Model: Cross-modal state prediction and fusion
    //
    // The world model enables:
    // - Counterfactual reasoning: "What if I had done X instead?"
    // - Policy evaluation: Simulate trajectories without acting
    // - Mental imagery: Generate internal sensory experiences
    // - Dreaming/consolidation: Offline simulation for learning
    //
    // THEORETICAL BASIS:
    // - DreamerV3: RSSM world model with symlog rewards
    // - JEPA (LeCun): Predict in latent space, not pixel space
    // - Active Inference (Friston): Generative model for EFE minimization
    //
    // Integrates with:
    // - Active Inference: Policy evaluation via world model simulation
    // - Imagination Engine: Scene generation using world model dynamics
    // - Hippocampus: Memory replay and consolidation
    // - Predictive Processing: Forward model for prediction errors
    // - Sleep System: Dreaming during offline consolidation
    //
    struct omni_world_model* omni_world_model;                        // Omni world model (forward/backward/lateral)
    struct nimcp_world_model* multimodal_world_model;                 // Multimodal world model for cross-modal fusion
    bool world_model_enabled;                                         // World model system enabled
    bool world_model_lazy_init;                                       // Defer world model initialization
    uint64_t last_world_model_update_us;                              // Last world model update timestamp
    uint64_t world_model_update_interval_us;                          // Update interval (default: 10ms)

    // =========================================================================
    // World Model Integration Bridges
    // =========================================================================
    // Enable bidirectional information flow between the omni world model
    // and brain subsystems for:
    // - Security-aware prediction with immune modulation
    // - Audit logging of world model operations
    // - Full cognitive layer integration
    // - Training layer synchronization
    // - Parietal spatial/physics reasoning
    // - Hypothalamic homeostatic control
    // - Thalamic attention gating
    // - Neural substrate metabolic constraints
    // - Memory system (hippocampus, engrams, consolidation)
    // - Knowledge Graph wiring system integration
    // - Theory of Mind social world modeling
    // - SNN/STDP/Plasticity direct integration
    //
    struct omni_wm_security_immune_bridge* wm_security_immune_bridge;  // Security + Immune cytokine modulation
    struct omni_wm_logging_bridge* wm_logging_bridge;                  // Audit logging integration
    struct omni_wm_cognitive_bridge* wm_cognitive_bridge;              // Full cognitive layer
    struct omni_wm_parietal_bridge* wm_parietal_bridge;                // Spatial/physics reasoning
    struct omni_wm_hypothalamus_bridge* wm_hypothalamus_bridge;        // Homeostatic control
    struct omni_wm_thalamic_bridge* wm_thalamic_bridge;                // Attention gating via nuclei
    struct omni_wm_substrate_bridge* wm_substrate_bridge;              // Metabolic constraints
    struct omni_wm_memory_bridge* wm_memory_bridge;                    // Hippocampus + engrams + consolidation
    struct omni_wm_kg_bridge* wm_kg_bridge;                            // Knowledge Graph wiring integration
    struct omni_wm_tom_bridge* wm_tom_bridge;                          // Theory of Mind social world modeling
    struct omni_wm_plasticity_bridge* wm_plasticity_bridge;            // SNN/STDP/Plasticity direct integration
    struct wm_thousand_brains_bridge* wm_thousand_brains_bridge;      // Hawkins Thousand Brains integration
    struct tb_integration_hub* tb_integration_hub;                     // Thousand Brains full-system integration hub

    // Thousand Brains core components (owned by integration hub lifecycle)
    struct column_ref_frame_manager* tb_ref_frames;                   // Grid cell reference frames
    struct column_voting_manager* tb_voting;                           // Column voting consensus
    struct dendritic_sequence_mgr* tb_sequences;                       // Dendritic sequence prediction

    // === INTUITIVE PHYSICS SUBSYSTEM ===
    //
    // Rigid-body physics engine for world model grounding:
    // - Object simulation (gravity, collisions, contacts, friction)
    // - Entity tracking (persistent object identity, object permanence)
    // - Scene graph (support/containment/proximity relations)
    // - Physics prior (constrains learned WM predictions with physical laws)
    //
    /* World model simulation engines — see appended fields at END of struct */

    // === FUZZY LOGIC INTEGRATION ===
    //
    // Fuzzy logic provides graded membership reasoning across all modules:
    // - Risk assessment with continuous risk grades
    // - Market regime detection with multi-membership
    // - Investor archetype heuristics with fuzzy evaluation
    // - Training convergence detection with fuzzy inference
    // - Plasticity rate scheduling via fuzzy rules
    //
    struct fuzzy_bridge* fuzzy_bridge;              // fuzzy_bridge_t* (opaque)
    bool fuzzy_logic_enabled;                       // Enable fuzzy logic integration

    // =========================================================================
    // CREATIVE SYSTEM INTEGRATION (Artistic Appreciation & Generation)
    // =========================================================================
    // The Creative System provides artistic cognition capabilities:
    // - Aesthetic Appreciation: Evaluate art quality (Berlyne aesthetics)
    // - Style Learning: Learn and represent artistic styles
    // - Content Generation: Generate text, music, visual art, video
    // - Multimodal Direction: Coordinate full creative projects (films)
    // - Ethics Validation: Ensure content safety and originality
    //
    // The creative orchestrator unifies:
    // - Appreciation Subsystem: Aesthetic evaluation, emotional response
    // - Inspiration Subsystem: Style embeddings, influence blending
    // - Generation Subsystem: Text, music, visual, video generators
    // - External Models: ONNX Runtime, diffusion, GANs, cloud APIs
    // - Validation Bridges: Quality, ethics, copyright, originality
    //
    // Integrates with:
    // - Emotion System: Aesthetic emotional responses (awe, sublime, joy)
    // - Memory System: Artistic experience storage and preferences
    // - Knowledge Graph: Art knowledge, style relationships
    // - Ethics Engine: Content safety and appropriateness
    // - Brain Immune System: Validation pipeline health
    // - Training System: Style learning and preference adaptation
    // - GPU Context: Accelerated generation via diffusion/GAN
    //
    struct creative_orchestrator* creative_orchestrator;    // Master orchestrator
    struct aesthetic_evaluator* aesthetic_evaluator;        // Aesthetic quality assessment
    struct style_representer* style_representation;         // Style embeddings/archetypes
    struct influence_blender* influence_blender;            // Multi-influence blending
    struct creative_pattern_extractor* creative_pattern_extractor; // Pattern extraction
    struct text_generator* text_generator;                  // Text/poetry generation
    struct music_generator* music_generator;                // Music composition
    struct visual_generator* visual_generator;              // Visual art generation
    struct video_generator* video_generator;                // Video/cinema generation
    struct multimodal_director* multimodal_director;        // Full project direction
    struct creative_bridge* creative_bridge;                // Validation pipeline
    struct creative_neural_bridge* creative_neural_bridge;  // Neural backend interface
    struct creative_ethics_bridge* creative_ethics_bridge;  // Ethics validation
    struct creative_training_bridge* creative_training_bridge; // Training/fine-tuning
    struct creative_emotion_bridge* creative_emotion_bridge; // Emotion-aesthetic bridge
    struct creative_memory_bridge* creative_memory_bridge;  // Artistic memory bridge
    struct creative_knowledge_bridge* creative_knowledge_bridge; // Art KG bridge
    struct style_perception* style_perception;              // Style recognition
    bool creative_enabled;                                  // Creative system enabled
    bool creative_lazy_init;                                // Defer initialization
    uint64_t last_creative_update_us;                       // Last update timestamp

    // === COGNITIVE OUTPUT RUBRIC ===
    //
    // Two-tier quality evaluation for brain_decide() outputs.
    // Rubric evaluator is lazy-initialized on first nimcp_brain_rubric() call.
    // last_decision caches the most recent brain_decide() result for rubric access.
    //
    struct rubric_evaluator* rubric_evaluator;   // Lazy-init rubric evaluator (NULL until first use)
    brain_decision_t* last_decision;             // Cached pointer to most recent decision

    // =========================================================================
    // MAMMILLARY BODIES INTEGRATION (Papez Circuit Memory Relay)
    // =========================================================================
    // The Mammillary Bodies are critical relay stations in the Papez circuit:
    // - Receive hippocampal input via fornix
    // - Relay to anterior thalamus via mammillothalamic tract
    // - Process head direction signals
    // - Support episodic memory consolidation
    //
    // Damage causes anterograde amnesia (Korsakoff syndrome).
    //
    // Integrates with:
    // - Hippocampus: Primary input via fornix
    // - Thalamus: Primary output via mammillothalamic tract
    // - Cingulate: Feedback for Papez circuit completion
    // - Training: Memory relay during learning
    //
    void* mammillary;                                // Mammillary bodies (nimcp_mammillary_t*, cast in brain_learning.c)
    bool mammillary_enabled;                         // Mammillary bodies enabled for this brain
    uint64_t last_mammillary_update_us;              // Last mammillary update timestamp

    // =========================================================================
    // VAE INTEGRATION (Variational Autoencoder)
    // =========================================================================
    // Learns compressed latent representations of input features.
    // Provides: anomaly detection, generative replay, FEP free energy.
    //
    // Integration points:
    // - Before learning: encode input → anomaly score (flag hard examples)
    // - During learning: train VAE (reconstruction + KL loss)
    // - After learning: latent representation for engram encoding
    //
    void* vae_system;                                // vae_system_t* (avoid header conflict)
    void* vae_training_bridge;                       // vae_training_bridge_t* (joint VAE+SNN training)
    bool vae_enabled;                                // VAE enabled for this brain
    float last_vae_anomaly_score;                    // Most recent anomaly score from VAE
    float last_vae_free_energy;                      // Most recent free energy (negative ELBO)

    // =========================================================================
    // COGNITIVE SUBSYSTEM TRAINING HOOKS
    // =========================================================================
    // Pointers to subsystem training engines that train during brain_learn_vector().
    // These ensure ALL cognitive modules learn alongside the primary network.
    //
    struct jepa_predictor* jepa_predictor;            // JEPA latent predictor (imagination foundation)
    bool jepa_predictor_enabled;                      // JEPA predictor training enabled
    struct predictive_hierarchy* pred_hierarchy;    // Predictive coding hierarchy
    bool pred_hierarchy_enabled;                      // Predictive hierarchy training enabled
    void* self_heal_engine;                           // self_heal_engine_t* (immune training)
    bool self_heal_enabled;                           // Self-heal training enabled
    uint32_t cognitive_train_interval;                // How often to train subsystems (default: every 5 steps)
    uint32_t cognitive_train_counter;                 // Internal counter for interval gating

    /* Per-module cognitive training statistics. _last_loss fields are
     * set from the underlying task's r->loss when it writes one; modules
     * whose underlying learners don't expose a scalar loss leave their
     * field at the NaN sentinel initialized by the brain factory. The
     * get_cognitive_stats API returns the sentinel unchanged so callers
     * can tell "no loss signal yet" from "actual zero loss". */
    struct {
        uint32_t grounded_lang_steps;
        float    grounded_lang_last_loss;
        uint32_t knowledge_steps;
        float    knowledge_last_loss;
        uint32_t vae_steps;
        float    vae_last_loss;
        uint32_t fep_parietal_steps;
        float    fep_parietal_last_loss;
        uint32_t physics_nn_steps;
        float    physics_nn_last_loss;
        uint32_t pred_hierarchy_steps;
        float    pred_hierarchy_last_loss;
        uint32_t jepa_steps;
        float    jepa_last_loss;
        uint32_t creative_steps;
        float    creative_last_loss;
        uint32_t self_heal_steps;
        float    self_heal_last_loss;
        uint32_t intuition_steps;
        float    intuition_last_loss;
        uint32_t fep_orchestrator_steps;
        float    fep_orchestrator_last_loss;
    } cognitive_stats;

    /* Module activity counters — populated by hot-path increments in
     * subsystems flagged as "statue suspects" (wired into hot path, but no
     * existing metric quantifies their action). Used by the ablation harness
     * to distinguish "module is firing" from "module is dead". All counters
     * are monotonic uint64; reset only on brain create. */
    struct {
        /* Glial — see src/core/brain/glial/* (G3/G4/G5 wiring) */
        uint64_t astrocyte_modulation_events;     /* synapse weight modulations applied */
        uint64_t oligodendrocyte_myelin_apply;    /* conduction-delay applies */
        uint64_t microglia_pruning_events;        /* synapse prunings */

        /* Sleep-wake — see src/core/brain/sleep/* */
        uint64_t sleep_replay_events;             /* memory replay invocations */
        uint64_t sleep_downscale_events;          /* synaptic homeostasis downscales */

        /* LGSS / safety gates — see src/security/lgss/*, src/core/brain/nimcp_brain_part_core.c
           (LGSS gates in brain_decide / brain_learn_vector) */
        uint64_t lgss_input_rejections;
        uint64_t lgss_action_blocks;
        uint64_t lgss_motor_blocks;
        uint64_t lgss_training_blocks;
        uint64_t lgss_reward_blocks;
        uint64_t ethics_violations;

        /* HNN forward path — does the LNN actually route through Hamiltonian-mode? */
        uint64_t hnn_forward_invocations;         /* lnn_layer_forward_hamiltonian calls */
        uint64_t hnn_fallback_invocations;        /* HNN failed -> fell back to LTC */

        /* Cortical column pool + ternary WTA — CC2-CC5 wiring */
        uint64_t cortical_column_forward_invocations;
        uint64_t cortical_wta_winners_total;      /* sum of winners across all calls */
        uint64_t cortical_wta_calls;              /* count of WTA invocations */

        /* Thalamic router */
        uint64_t thalamic_routes_dispatched;
        uint64_t thalamic_drops_backpressure;

        /* Hemispheric callosum */
        uint64_t callosum_transfers;

        /* KG hot-path consumers — W16/W17 */
        uint64_t kg_consumer_hits;                /* brain_decide / brain_learn_vector consults */

        /* Reserved for future */
        uint64_t reserved[8];
    } module_activity;

    // =========================================================================
    // COGNITIVE DISPATCH METRICS (Parallel Actor Pattern)
    // =========================================================================
    // Updated by brain_train_cognitive_parallel() after each dispatch.
    // Read by probe sampler for monitoring dashboard.
    struct {
        uint64_t last_dispatch_us;       /**< Wall-clock for last parallel dispatch */
        uint32_t last_modules_executed;  /**< Modules that actually ran in last dispatch */
        uint32_t last_modules_submitted; /**< Modules submitted to pool */
        uint64_t total_dispatches;       /**< Total parallel dispatch calls */
        uint64_t total_sequential;       /**< Total sequential fallback calls */
        uint64_t cumulative_dispatch_us; /**< Total time in parallel dispatch */
        uint32_t pool_thread_count;      /**< Thread pool size (0 = sequential) */
        float    slowest_module_us;      /**< Slowest module in last dispatch */
        float    avg_module_us;          /**< Average module time in last dispatch */
    } dispatch_metrics;

    // =========================================================================
    // COGNITIVE TRANSCRIPT (Communication Layer Phase 1)
    // =========================================================================
    // Cached transcript from the most recent brain_decide() call.
    // Owned by the brain; replaced on each decide call.
    struct cognitive_transcript* last_transcript;

    // =========================================================================
    // WHITE MATTER TRACTS (Long-Range Myelinated Connectivity)
    // =========================================================================
    // Models 8 major white matter tracts with conduction velocity, myelination,
    // and signal delay. Foundation for inter-regional communication timing.
    // - Corpus Callosum: Interhemispheric transfer
    // - Arcuate Fasciculus: Broca↔Wernicke language pathway
    // - Corticospinal: Motor cortex → spinal cord descending control
    // - Spinothalamic: Spinal cord → thalamus ascending pain/temperature
    // - Optic Radiation: LGN → V1 visual pathway
    //
    struct wmt_system* white_matter;                     // White matter tract system
    bool white_matter_enabled;                           // White matter enabled for this brain
    uint64_t last_white_matter_update_us;                // Last white matter update timestamp

    // =========================================================================
    // INFERIOR COLLICULUS (Auditory Midbrain Processing)
    // =========================================================================
    // Primary auditory midbrain nucleus providing:
    // - Tonotopic organization (64 log-spaced frequency channels, 20Hz-20kHz)
    // - Binaural integration: ITD/ILD for sound localization (azimuth/elevation)
    // - ICC central nucleus: ascending auditory relay
    // - ICX external nucleus: multisensory integration
    // Relay station between cochlear nucleus and MGN of thalamus.
    //
    struct inferior_colliculus* inferior_colliculus;      // Inferior colliculus system
    bool inferior_colliculus_enabled;                     // IC enabled for this brain
    uint64_t last_inferior_colliculus_update_us;          // Last IC update timestamp

    // =========================================================================
    // SPINAL CORD / MOTOR OUTPUT (Effector Interface)
    // =========================================================================
    // Final common pathway for motor output:
    // - Central Pattern Generators (CPGs) for rhythmic movement
    // - Reflex arcs: stretch, withdrawal, crossed extension
    // - Motor neuron pools grouped by effector
    // - Proprioceptive feedback: Ia, II, Ib afferents
    // - Gate control for pain modulation
    // - Descending input: corticospinal, rubrospinal, vestibulospinal
    //
    struct spinal_cord* spinal_cord;                     // Spinal cord motor output system
    bool spinal_cord_enabled;                            // Spinal cord enabled for this brain
    uint64_t last_spinal_cord_update_us;                 // Last spinal cord update timestamp

    // =========================================================================
    // CORTICAL INTERNEURONS (Inhibitory Microcircuit Control)
    // =========================================================================
    // GABAergic interneurons controlling cortical microcircuit dynamics:
    // - PV+ Basket: Fast-spiking, perisomatic inhibition, gamma oscillations
    // - PV+ Chandelier: Axo-axonic, gates AP initiation at AIS
    // - SST+ Martinotti: Dendrite-targeting, feedback inhibition L5→L1
    // - VIP+: Disinhibition (inhibit SST/PV), attention gating
    // - NGF L1: Neurogliaform, volume transmission GABA, slow inhibition
    // Critical for E/I balance, gamma oscillations, and information gating.
    //
    struct cortical_interneuron_system* cortical_interneurons;  // Cortical interneuron system
    bool cortical_interneurons_enabled;                         // Interneurons enabled
    uint64_t last_cortical_interneurons_update_us;              // Last update timestamp

    // =========================================================================
    // NEUROPEPTIDE SYSTEM (Slow Neuromodulation)
    // =========================================================================
    // 8 neuropeptide systems with synthesis, release, degradation kinetics:
    // - Oxytocin (bonding), Vasopressin (aggression, pair bonding)
    // - NPY (appetite, anxiolysis), Substance P (pain, inflammation)
    // - Orexin (wakefulness), CRH (stress/HPA axis)
    // - Endorphin (pain relief, euphoria), CCK (satiety, anxiety)
    // Slow, long-lasting modulatory effects complementing classical monoamines.
    //
    struct neuropeptide_system* neuropeptide;             // Neuropeptide system
    bool neuropeptide_enabled;                            // Neuropeptide system enabled
    uint64_t last_neuropeptide_update_us;                 // Last neuropeptide update timestamp

    // =========================================================================
    // ENDOCANNABINOID SYSTEM (Retrograde Synaptic Modulation)
    // =========================================================================
    // CB1/CB2 receptors with 2-AG and anandamide (AEA) endocannabinoids:
    // - Retrograde signaling: postsynaptic → presynaptic suppression
    // - DSI/DSE: Depolarization-induced suppression of inhibition/excitation
    // - Pain modulation via CB1 on spinal/PAG interneurons
    // - Tonic inhibition: baseline CB1-mediated neural suppression
    // Modulates pain, mood, appetite, memory consolidation.
    //
    struct endocannabinoid_system* endocannabinoid;      // Endocannabinoid system
    bool endocannabinoid_enabled;                        // ECB enabled for this brain
    uint64_t last_endocannabinoid_update_us;             // Last ECB update timestamp

    // =========================================================================
    // GLYMPHATIC SYSTEM (Brain Waste Clearance)
    // =========================================================================
    // Astrocyte-lined perivascular channels clearing metabolic waste:
    // - AQP4 (aquaporin-4) channels drive CSF/ISF exchange
    // - 10-60x more active during NREM sleep
    // - Clears beta-amyloid, tau protein, metabolic waste
    // - Interstitial space expands ~60% during sleep
    // - State machine: INACTIVE → PRIMING → ACTIVE → FLUSHING
    // High waste degrades learning rate and decision confidence.
    //
    struct glymphatic_system* glymphatic;                // Glymphatic waste clearance system
    bool glymphatic_enabled;                             // Glymphatic enabled for this brain
    uint64_t last_glymphatic_update_us;                  // Last glymphatic update timestamp

    // === UNIFIED EXPERIENCE API (Developmental Learning) ===
    //
    // brain_experience() merges inference + learning into a single call.
    // Every perception triggers both prediction and attention-gated plasticity.
    //
    brain_experience_config_t experience_config;    // Experience learning configuration
    bool inference_learning_enabled;                // Master switch for inference-time learning
    float* last_experience_prediction;              // Cached output from last experience (for prediction error)
    float* last_experience_input;                   // Cached input from last experience (for correction)
    uint32_t last_experience_input_size;            // Size of cached input
    uint32_t last_experience_output_size;           // Size of cached output
    uint64_t experience_count;                      // Monotonic experience counter
    uint64_t synaptogenesis_count;                  // Total new synapses formed via experience

    // =========================================================================
    // HEMISPHERIC ARCHITECTURE (Callosum + Lateralization)
    // =========================================================================
    // Lightweight integration: corpus callosum channels + lateralization routing
    // without creating full sub-brains (avoids 3x memory cost).
    // Callosum provides inter-hemispheric signal transfer with bandwidth/latency.
    // Lateralization routes cognitive domains to dominant hemisphere.
    // Neurons are logically split: first half = left, second half = right.
    //
    corpus_callosum_t* callosum;                     // Inter-hemispheric bridge (5 channels)
    lateralization_profile_t lateralization;          // Domain-to-hemisphere routing weights
    hemisphere_id_t dominant_hemisphere;              // Current overall dominant hemisphere
    bool hemispheric_enabled;                        // Master enable flag
    float hemispheric_balance;                       // Left-right activity balance [-1=left, +1=right]
    uint64_t last_callosum_process_us;               // Last callosum queue processing timestamp

    // === EDGE-CLOUD HYBRID INFERENCE ===
    //
    // Confidence-gated routing between local (edge) and cloud (backend) brains.
    // System 1/2 analogy: fast local inference for easy cases, cloud escalation
    // for hard cases, with distillation to make the local brain smarter over time.
    //
    struct cloud_inference_bridge* cloud_bridge;     // Edge-cloud inference bridge (NULL if standalone)
    bool cloud_inference_enabled;                    // Master enable flag

    // === RECURRENT FORWARD PASS (Iterative Refinement) ===
    //
    // Multiple forward passes with output-to-input feedback for harder problems.
    // Biological: Recurrent thalamocortical loops refine percepts over 50-150ms.
    // Low-confidence outputs trigger re-processing with output blended into input.
    //
    bool recurrent_enabled;                          // Master enable flag
    uint32_t recurrent_max_iterations;               // Max refinement iterations (default: 3)
    float recurrent_confidence_threshold;            // Stop iterating if confidence exceeds this (default: 0.7)
    float recurrent_blend_alpha;                     // Output-to-input blend ratio (default: 0.3)
    uint32_t recurrent_iteration_count;              // Last decision's iteration count (diagnostic)

    // === BPTT (Backpropagation Through Time) ===
    //
    // Circular buffer of recent (input, output, target) tuples for temporal
    // gradient accumulation. Enables learning from sequential dependencies.
    // Biological: Hippocampal replay + eligibility traces over recent experience.
    //
    bool bptt_enabled;                               // Master enable flag
    uint32_t bptt_window_size;                       // Temporal window (default: 8)
    float bptt_discount;                             // Gradient discount per step back (default: 0.9)
    struct {
        float* input;                                // Input features [num_inputs]
        float* output;                               // Network output [num_outputs]
        float* target;                               // Target vector [num_outputs]
        float loss;                                  // Loss at this timestep
    } *bptt_buffer;                                  // Circular buffer [bptt_window_size]
    uint32_t bptt_head;                              // Next write position
    uint32_t bptt_count;                             // Number of valid entries (0..window_size)
    uint32_t bptt_input_dim;                         // Cached input dimension for buffer sizing
    uint32_t bptt_output_dim;                        // Cached output dimension for buffer sizing

    // === PRE-ALLOCATED SCRATCH BUFFERS FOR learn() HOT PATH ===
    //
    // Eliminates 4 malloc+free round-trips per learn() call by reusing
    // pre-allocated buffers. Buffers grow on first use to match brain dimensions.
    //
    struct {
        float* target;              // One-hot target vector [num_outputs]
        float* prediction;          // Post-learn prediction [num_outputs]
        float* attended_features;   // Attention output [num_inputs]
        uint32_t* cue_neurons;      // Engram recall cue IDs [num_inputs]
        uint32_t target_cap;        // Capacity of target/prediction buffers
        uint32_t features_cap;      // Capacity of attended_features/cue_neurons
    } learn_scratch;

    // === OPTIMIZATION: PLASTICITY INTERVAL GATING ===
    //
    // Biological plasticity updates (TPB, EDP, structural, coordinator) are
    // expensive and don't need to run every single training step. Gate them
    // to run every N steps for a significant speedup with minimal accuracy impact.
    //
    uint32_t plasticity_step_counter;            // Counter for interval gating (run every 10 steps)

    // === OPTIMIZATION: PRE-ALLOCATED INFERENCE OUTPUT BUFFERS ===
    //
    // Eliminates 3 nimcp_calloc + nimcp_free round-trips per forward pass
    // by reusing pre-allocated buffers for adaptive/cnn/snn outputs.
    //
    float* inference_buf_adaptive;               // Pre-allocated adaptive output [num_outputs]
    float* inference_buf_cnn;                    // Pre-allocated CNN output [num_outputs]
    float* inference_buf_snn;                    // Pre-allocated SNN output [num_outputs]
    uint32_t inference_buf_size;                 // Current buffer capacity (num_outputs)

    // === MULTI-NETWORK FUSION (Phase 6: Architecture Eval) ===
    bool enable_fusion;                          // Weighted ensemble of network outputs
    float fusion_weights[4];                     // [adaptive, CNN, SNN, LNN] (softmax-normalized)
    float* fusion_scratch_lnn;                   // Scratch buffer for LNN output [num_outputs]

    // === EDGE/ROBOT INTEGRATION ===
    //
    // Sensor hub, safety watchdog, and ROS 2 bridge for embodied deployment.
    // Uses void* to avoid header dependency — cast in .c files that use them.
    //
    // - Sensor Hub: Aggregates heterogeneous sensor readings (LIDAR, IMU, etc.)
    //   into composed feature vectors for brain inference.
    // - Safety Watchdog: Monitors brain output for NaN/Inf/magnitude violations
    //   and enforces heartbeat-based deadman switch for actuator safety.
    // - ROS 2 Bridge: Connects brain to Robot Operating System 2 for real
    //   robot platforms (stub mode when ROS 2 not available).
    //
    void* sensor_hub;                            // nimcp_sensor_hub_t* — unified sensor interface
    void* safety_watchdog;                       // nimcp_safety_watchdog_t* — actuator safety
    void* ros2_bridge;                           // nimcp_ros2_bridge_t* — ROS 2 integration
    bool sensor_hub_enabled;
    bool safety_watchdog_enabled;
    bool ros2_bridge_enabled;

    // === SWARM BRIDGE INTEGRATION ===
    //
    // Bridges connecting swarm coordination with individual subsystems:
    // - Portia-Swarm: Resource-adaptive intelligence ↔ collective decisions
    // - Dragonfly-Swarm: Target tracking ↔ multi-drone pursuit coordination
    //
    void* portia_swarm_bridge;                   // portia_swarm_bridge_t*
    bool portia_swarm_bridge_enabled;
    void* swarm_dragonfly_bridge;                // swarm_dragonfly_bridge_t*
    bool swarm_dragonfly_bridge_enabled;

    // === OCTOPUS COGNITIVE MODULE (Phase 1 — 2026-04-20) ===
    //
    // Distributed peripheral cognition: N semi-autonomous arm-agents each
    // processing a slice of input, reporting back to a central aggregator.
    // See include/cognitive/octopus/nimcp_octopus.h for API + rationale.
    //
    // Integration bridges (swarm/immune/ethics/world/fep/bio-async) will
    // register hooks via octopus_set_*_hook(). Phase 1 only carries the
    // core field + lifecycle; bridges land in subsequent phases.
    void* octopus;                               // octopus_system_t*
    bool octopus_enabled;
    void* octopus_bridge_state;                  // octopus_bridge_state_t* (Phase 2a bridges)

    // === AMYGDALA (Phase 3c — 2026-04-20) ===
    // Fear / emotional-salience subsystem. The octopus bridges read
    // brain->amygdala for fear responses to modulate arm decisions.
    // See include/core/brain/subcortical/nimcp_amygdala.h for API.
    void* amygdala;                              // amygdala_t*
    bool amygdala_enabled;

    // === FLIGHT CONTROLLER BRIDGES ===
    // Standalone companion-computer modules. Created by edge init when
    // config flags are set. Feed telemetry into sensor_hub for brain input.
    void* mavlink_bridge;                        // nimcp_mavlink_bridge_t*
    void* dji_bridge;                            // nimcp_dji_bridge_t*
    void* msp_bridge;                            // nimcp_msp_bridge_t*
    void* parrot_bridge;                         // nimcp_parrot_bridge_t*

    // === SENSORIMOTOR + LANGUAGE ===
    void* sensorimotor;                          // nimcp_sensorimotor_t*
    void* native_language;                       // nimcp_native_language_t*
    void* brain_tokenizer;                       // nimcp_tokenizer_t* (brain-native, distinct from generation tokenizer)
    bool native_language_enabled;
    bool brain_tokenizer_enabled;

    // === COGNITIVE ENHANCEMENTS ===
    void* inner_speech;                          // nimcp_inner_speech_t*
    void* episodic_replay;                       // nimcp_episodic_replay_t*
    void* world_model_trainer;                   // nimcp_wmt_t*
    void* output_attention;                      // nimcp_oa_t*
    void* wm_scratchpad;                         // nimcp_wms_t*
    void* analogical_transfer;                   // nimcp_analogical_t*
    void* multiscale_memory;                     // nimcp_multiscale_t*
    void* emotional_learning;                    // nimcp_emotional_learning_t*
    void* contrastive_self;                      // nimcp_contrastive_self_t*
    void* self_curriculum;                       // nimcp_self_curriculum_t*
    void* dynamic_arch;                          // nimcp_dynamic_arch_t*

    /* === APPENDED FIELDS (checkpoint-safe: brain_struct is calloc'd, not serialized) === */
    struct intuitive_physics_engine* intuitive_physics;
    struct entity_tracker* entity_tracker;
    struct scene_graph* scene_graph;
    struct physics_prior* physics_prior;
    struct chemistry_sim* chemistry_sim;
    struct biology_sim* biology_sim;
    struct world_prior* world_prior;
    bool intuitive_physics_enabled;

    /* === TRAINING DASHBOARD METRICS ===
     * Written by training script via set_training_metrics API.
     * Read by metrics exporter via get_training_metrics API.
     * All fields zero-initialized by calloc. */
    struct {
        /* Per-domain learning rates */
        float lr_physics;
        float lr_chemistry;
        float lr_biology;
        /* World model */
        uint32_t wm_steps;
        uint32_t wm_physics_transitions;
        uint32_t wm_chemistry_transitions;
        uint32_t wm_biology_transitions;
        /* Curriculum */
        float fact_ratio;
        char current_domain[64];
        bool warm_start_complete;
        uint32_t warm_start_step;
        /* Collapse detection */
        uint32_t collapse_events;
        /* Surprise / replay */
        uint32_t surprises_stored;
        uint32_t replays_done;
        /* Language */
        uint32_t vocab_size;
        float language_confidence;
        /* General */
        uint32_t current_stage;
        uint32_t current_step;
        uint32_t active_engines;
    } training_dashboard;

    /* === UNIFIED PROBE SYSTEM ===
     * NULL until first probe created (lazy init via eager_init_cognitive or API).
     * See include/core/probes/nimcp_brain_probes.h */
    struct probe_registry* probe_registry;

    /* === CHECKPOINT PROVENANCE ===
     * Path the brain was loaded from, set by brain_load_auto() during
     * checkpoint restoration. Empty string for fresh brains. Used by
     * nimcp_brain_eager_init_cognitive() to defer post-init sidecar loads
     * (.gl_lang, .immune, .kg) until AFTER grounded_lang/immune_system/
     * internal_kg actually exist — they are NULL during brain_load() itself.
     *
     * NIMCP_METRICS_PATH_SIZE (= 512) matches the buffer used by the
     * sidecar load helpers in nimcp_brain_persistence.c. */
    char loaded_from_path[512];

    /* === BROCA PRAGMATICS PROCESSOR (indirect speech-act detection) ===
     * Pragmatics processor sibling of broca_adapter — created when Broca
     * is initialized; used by the communication cascade Stage 2 (Goal) to
     * reclassify "Can you X?" questions as REQUEST speech acts rather than
     * literal yes/no questions. Forward-declared opaque type — full impl
     * in src/core/brain/regions/broca/nimcp_pragmatics_processor.c.
     * Lifetime: created in nimcp_brain_factory_init_broca_subsystem,
     * destroyed in nimcp_brain_factory_destroy_broca_subsystem. NULL when
     * Broca is disabled. APPENDED at end of brain_struct to avoid ABI
     * shift on existing fields. */
    struct pragmatics_processor* broca_pragmatics;

    /* === CASCADE SELF-TRAIN (Wave 2 Item #10) ===
     * Runtime training hook that closes the loop on the cascade's
     * Phase 2D-B self_match signal. When enabled, every successful
     * cascade run calls snn_language_bridge_echo_correct() on each
     * produced word with a learning rate scaled by (self_match - baseline).
     * The baseline is an EMA over recent self_match values, so the
     * reward signal is roughly mean-centered — biological reward
     * prediction error (R-PE), three-factor learning with dopamine-style
     * gating built on top of the bridge's existing eligibility traces.
     *
     * Default OFF (calloc zero). Opt-in via
     * nimcp_brain_set_cascade_self_train_enabled(). All four fields are
     * APPENDED to brain_struct to avoid ABI shifts on existing layouts.
     *
     *   cascade_self_train_enabled — master gate.
     *   cascade_self_train_baseline — EMA of self_match across calls.
     *   cascade_self_train_alpha — EMA mixing rate (default 0.05).
     *   cascade_self_train_lr_scale — multiplier on echo_correct lr_scale.
     *
     * Concurrency: the cascade orchestrator is normally called from a
     * single thread per brain. The EMA update is a read-modify-write
     * on a single float; if a future caller drives cascade concurrently
     * the EMA will be slightly noisy but the training is monotonic in
     * reward sign so it's not a correctness issue. We deliberately do
     * NOT take a mutex here to avoid introducing a new lock ordering
     * relative to bridge plasticity calls. */
    bool  cascade_self_train_enabled;
    float cascade_self_train_baseline;
    float cascade_self_train_alpha;
    float cascade_self_train_lr_scale;

    /* === ARCUATE FASCICULUS FEEDBACK (Slice 2, 2026-05-18) ===
     * Bidirectional Wernicke↔Broca channel. Real anatomy: the arcuate
     * fasciculus carries Wernicke's phonological/semantic parse of own
     * speech back to Broca's motor + syntactic planning. Conduction
     * aphasia (arcuate damage) → can repeat external speech but can't
     * self-monitor → exactly the failure mode the bridge had before.
     *
     * Within the recurrent cascade, between iterations:
     *   1. comprehend(iter_N_utterance) → output_semantic_vec
     *   2. arcuate_feedback_vec[i] = (content_intent[i] - output_semantic_vec[i]) * (1 - self_match)
     *      i.e. the "missing semantic content" weighted by mismatch
     *   3. cascade_stage_content reads arcuate_feedback_vec at top of
     *      iter N+1 and ADDS it to content_intent — the brain
     *      effectively "tries harder" on the dimensions it just missed.
     *
     * NULL/dim=0 outside of an active recurrent run. Lifetime owned by
     * communication_cascade_run_recurrent (alloc on first non-zero
     * write, free on exit). Not thread-safe — produce_cascade_recurrent
     * is single-caller-at-a-time by contract. */
    float*   arcuate_feedback_vec;
    uint32_t arcuate_feedback_dim;
    float    arcuate_feedback_strength;     /* 0..1; multiplied into the add */

    /* S2-H2 fix (2026-05-19) — convex-blend arcuate feedback.
     *
     * Pre-fix the apply step in cascade_stage_content was
     *   content_intent[i] += k * feedback_vec[i]
     * with feedback_vec = (intent - own_output). On a stable prompt where
     * the next iter's intent ≈ blend ≈ prev intent, this iterates to
     *   new_intent ≈ (1+k)*old_intent - k*own_output
     * — i.e. an AMPLIFICATION of intent by factor (1+k) per iter, not an
     * error-correction step. Bridge confidence climbing 2-4× across
     * iterations turned out to be magnitude growth, not settling.
     *
     * Post-fix: the recurrent loop snapshots iter-0's content_intent as
     * a FIXED target into arcuate_target_vec, then recomputes
     * feedback_vec = (target - own_output) each iter, and the apply step
     * does a convex blend instead of additive:
     *   content_intent[i] = (1 - blend) * content_intent[i] + blend * (content_intent[i] + k * feedback_vec[i])
     * which collapses to
     *   content_intent[i] += blend * k * feedback_vec[i]
     * but bounded by `blend` so magnitude can't grow without bound.
     *
     * arcuate_feedback_blend: convex blend coefficient in [0, 1].
     *   0.0 => fully retain pre-arcuate content_intent (no correction)
     *   1.0 => fully accept the corrected value (legacy additive behavior at k)
     *   Recurrent loop sets this to a small value (default 0.5) so each
     *   iter only takes a half-step toward the corrected intent. When 0
     *   (default, single-pass cascade), cascade_stage_content treats
     *   apply as a no-op even if arcuate_feedback_vec is set — keeps
     *   the recurrent-only contract explicit.
     *
     * arcuate_target_vec: heap-allocated snapshot of iter-0's
     *   content_intent. Owned by communication_cascade_run_recurrent
     *   (alloc on iter==0, free on exit). Sized to gl_dim. NULL outside
     *   an active recurrent run. */
    float    arcuate_feedback_blend;
    float*   arcuate_target_vec;
    uint32_t arcuate_target_dim;

    /* === SPEECH REPAIR PROCESSOR (statue wiring 2026-05-11) ===
     * Disfluency cleaner used by the communication cascade just before
     * Stage 8 (self-comprehension). The brain's own utterance is run
     * through speech_repair_clean() to strip filled pauses, repetitions,
     * and false starts so the self_match score reflects the intended
     * content, not the production noise. APPENDED at end of brain_struct
     * to avoid ABI shift on existing layouts. Lifetime: created in
     * nimcp_brain_factory_init_broca_subsystem alongside the pragmatics
     * processor, destroyed in nimcp_brain_factory_destroy_broca_subsystem.
     * NULL when Broca is disabled — the cascade tolerates NULL by simply
     * skipping the repair step. Forward-declared opaque type. */
    struct speech_repair* speech_repair;

    /* === CASCADE TELEMETRY COUNTERS (Batch K, 2026-05-12) ===
     * Lifetime totals incremented by the cascade orchestrator. _Atomic
     * so the RO socket thread pool can snapshot them concurrently with
     * a running cascade. Stored inline (not heap-allocated) so the
     * counters survive across save/load — the brain_struct itself isn't
     * persisted, but the trainer only needs run-to-run continuity within
     * a single daemon process.
     *
     * APPENDED at end of brain_struct — ABI append-only. See
     * include/language/nimcp_communication_cascade.h for the public
     * snapshot type nimcp_cascade_counters_t. */
    _Atomic uint64_t cascade_total_runs;
    _Atomic uint64_t cascade_runs_with_prompt;
    _Atomic uint64_t cascade_runs_spontaneous;
    _Atomic uint64_t cascade_runs_fatal_error;
    _Atomic uint64_t cascade_stage_invocations[15];
    _Atomic uint64_t cascade_stage_mask_skips[15];
    _Atomic uint64_t cascade_stage_failures[15];
    _Atomic uint64_t cascade_pragmatics_indirect_overrides;
    _Atomic uint64_t cascade_wernicke_lexicon_miss;
    _Atomic uint64_t cascade_speech_repair_applied;
    _Atomic uint64_t cascade_self_train_steps_matched;
    _Atomic uint64_t cascade_self_train_steps_no_bindings;
    _Atomic uint64_t cascade_self_produced_events_fired;
    _Atomic uint64_t cascade_discourse_ring_pushes_user;
    _Atomic uint64_t cascade_discourse_ring_pushes_self;
    /* S1-H3+H4 fix (2026-05-19) — count of times the recurrent loop hit
     * OOM allocating its convergence-check / arcuate-feedback buffers and
     * had to fall back to degraded paths. Surfaces as
     * cascade_recurrent_oom_count in counter snapshots. Non-zero indicates
     * the recurrent cascade was running with reduced convergence
     * detection — useful when investigating "loop ran to max_iters but
     * shouldn't have". Relaxed atomic; single writer (recurrent loop)
     * but multiple-reader from the RO snapshot path. */
    _Atomic uint64_t cascade_recurrent_oom_count;

    /* S3-H3 fix (2026-05-19): cascade_stage_lexical hit the FEP fallback
     * because prod.semantic_vector was NULL. Older bridges don't fill it;
     * non-zero indicates the FEP pe_total is operating with the lexical
     * channel zeroed and precision-weighting is missing that signal. */
    _Atomic uint64_t cascade_fep_lexical_skipped;

    /* S3-H6 fix (2026-05-19): cascade_stage_self_train computed an
     * effective lr_scale (= base * gate * precision) that hit the
     * CASCADE_SELF_TRAIN_LR_SCALE_MAX cap. Non-zero means the cap is
     * engaging — the intended precision-boost is being clipped and the
     * brain might be missing some plasticity headroom. */
    _Atomic uint64_t cascade_self_train_precision_cap_hits;

    /* Audit Category A item 1 — route nimcp_brain_grounded_respond
     * through the 15-stage cascade orchestrator instead of the legacy
     * bridge-softmax passthrough. Default OFF preserves bit-for-bit
     * legacy behavior. When ON, respond builds a cascade state with
     * the input as prompt, runs all stages, and returns state->utterance
     * (cleaned by speech_repair, validated by self-comprehension, etc).
     *
     * The cascade is significantly slower per call than the bridge-only
     * path — opt-in by deployment as a quality-vs-latency tradeoff. */
    bool respond_via_cascade;

    /* === SLICE 6: THALAMIC GATING OF CASCADE-STAGE BANDWIDTH (2026-05-18) ===
     * Real thalamus is the central relay + gain control of the brain.
     * The pulvinar in particular modulates attention by gain-controlling
     * cortical activity — boosting attended regions, suppressing
     * distractors. This slice adds per-stage gates that scale each
     * stage's CONTRIBUTION based on the brain's current arousal/
     * attention state.
     *
     * thalamic_gate_enabled: master switch. Default OFF (calloc zero).
     * thalamic_gate_weights[15]: per-stage scalar gain in [0.0, 1.0].
     * thalamic_gate_manual_override[15]: per-stage manual-override flag.
     *
     * APPENDED at end of brain_struct; race-tolerant like self_train EMA. */
    bool  thalamic_gate_enabled;
    float thalamic_gate_weights[15];
    bool  thalamic_gate_manual_override[15];

    /* === SLICE 3 — FEP RECURRENT METRICS (2026-05-18) ===
     * Per-iteration prediction-error trace + summary scalars written
     * by communication_cascade_run_recurrent at exit. Read by
     * nimcp_brain_get_cascade_fep_metrics_impl. Not _Atomic — the
     * recurrent loop is single-caller-at-a-time by contract.
     *
     * fep_pe_trace sized to 64 = max iteration cap inside
     * communication_cascade_run_recurrent. */
    uint32_t fep_iterations_run;
    float    fep_pe_trace[64];
    float    fep_pe_initial;
    float    fep_pe_terminal;
    float    fep_pe_min;
    float    fep_pe_max;
    float    fep_pe_mean;
    float    fep_pe_decay_rate;
    int      fep_converged;

    /* S2-H2 fix (2026-05-19) — ||content_intent|| per iter trace.
     * Lets external monitors detect runaway amplification regardless of
     * the convex-blend fix above (defense in depth). Same lifetime as
     * fep_pe_trace: filled at recurrent-loop exit, survives across calls
     * for a single snapshot. */
    float    fep_intent_norm_trace[64];
    float    fep_intent_norm_initial;
    float    fep_intent_norm_terminal;
    float    fep_intent_norm_max;

    /* === SLICE 5 — PHONOLOGICAL LOOP working memory buffer (2026-05-18) ===
     * Baddeley's working memory model: a short-term phonological store
     * (~2 seconds of speech) plus an articulatory rehearsal mechanism
     * that refreshes activation. Real speakers don't construct an
     * utterance in one shot — they hold a partial draft, decay it
     * passively, and refresh it through rehearsal.
     *
     * Used by communication_cascade_run_recurrent (Slice 5):
     *   - At the start of each iteration, decay all phonemic traces
     *     by `loop_decay_rate`; drop any whose trace < 0.05.
     *   - After cascade_stage_lexical produces new words, merge them
     *     into the buffer: existing word → refresh trace to 1.0,
     *     new word → append (cap at loop_max_words).
     *   - cascade_stage_syntactic / Broca read the buffer's surface
     *     form (words with trace >= 0.3) as the "intended utterance".
     *   - cascade_stage_self_comprehension parses the buffer's surface
     *     form (not the one-shot lexical output).
     *
     * Default OFF — when loop_enabled=false, the recurrent cascade
     * behaves byte-identically to Slice 1+2 (no buffer interaction).
     *
     * Memory: heap-allocated buffer + trace + words arrays, owned by
     * the brain across its lifetime; created in brain init alongside
     * speech_repair, freed in brain destroy. Capacity grows on demand
     * (realloc); never shrinks. Failures during realloc are non-fatal —
     * the loop truncates writes rather than crashing.
     *
     * Lock: loop_mutex protects buffer/traces/words/last_refresh_ms.
     * Held only by accessor functions. NEVER acquire while holding
     * bridge or broca mutexes — see lock-ordering.md. The recurrent
     * cascade calls merge/read APIs sequentially, so contention is
     * single-caller-at-a-time in practice. void* opaque storage avoids
     * pulling thread.h into the brain header. */
    bool      loop_enabled;
    char*     loop_buffer;             /* accumulated utterance text, NUL-terminated */
    uint32_t  loop_buffer_len;         /* strlen() — excludes NUL */
    uint32_t  loop_buffer_capacity;    /* bytes allocated for buffer */
    float*    loop_phonemic_trace;     /* per-word activation/decay traces */
    uint32_t  loop_trace_count;        /* current number of valid traces */
    uint32_t  loop_trace_capacity;     /* slots allocated for traces */
    uint32_t  loop_max_words;          /* cap at this many words (default 16) */
    float     loop_decay_rate;         /* per-iteration decay (default 0.15) */
    uint64_t  loop_last_refresh_ms;    /* monotonic ms — for rehearsal timing */
    void*     loop_mutex;              /* nimcp_mutex_t* — see lock comment */
    char**    loop_words;              /* per-word storage; size = loop_trace_capacity */

    /* S5-H3/H5 fix (2026-05-19): operator-controllable surface threshold.
     * Pre-fix the internal rerender used the 0.05 prune threshold (which
     * matched eviction) so `loop_buffer` text held EVERY trace, but
     * `phonological_loop_render_active` defaulted to 0.3 — trainers reading
     * diag (loop_buffer_len) saw MORE words than the cascade actually
     * emitted. Words with trace ∈ [0.05, 0.3) sat in the silent decay band:
     * present in the buffer, invisible to the cascade.
     *
     * Post-fix: both internal rerender + render_active use this field.
     * Default 0.3 matches the legacy cascade behavior; operators may lower
     * (e.g. to 0.05) to make the silent band observable or raise (e.g. 0.5)
     * to tighten the surface form. Clamped to [0.0, 1.0]. Setter:
     * nimcp_brain_set_phonological_loop_threshold. */
    float     loop_surface_threshold;   /* default 0.3; clamped [0,1] */

    /* === SLICE 7 — CEREBELLAR PREDICTION-CORRECTION (2026-05-18) ===
     * Wires the existing cerebellum adapter (brain->cerebellum, opaque
     * cerebellum_adapter_t* — see
     * include/core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h)
     * into the cascade's motor + prosody stages.
     *
     * Biological role: the cerebellum predicts the next motor pattern
     * ~100ms before execution, computes prediction error vs. observed,
     * and corrects motor + prosody trajectories on the fly. In speech
     * production this is what smooths syllable timing + prosodic
     * contours and corrects articulator dynamics. Cerebellar damage
     * produces ataxic dysarthria (slurred, scanning, irregular speech).
     *
     * Default OFF. When ON, cascade_stage_motor and cascade_stage_prosody:
     *   1. Build an 8D feature vector from cascade state (drive,
     *      syntactic, phonological).
     *   2. Call cerebellum_predict_outcome to get predicted next motor /
     *      prosody pattern.
     *   3. Run the stage's existing logic (always).
     *   4. Build the "actual" 8D feature vector from realised outputs.
     *   5. Call cerebellum_update_forward_model to learn from the
     *      (command, outcome) pair + cerebellum_broadcast_error so
     *      Purkinje LTD fires on high-error climbing-fiber signals.
     *   6. Stash prediction-error norm on cascade state.
     *
     * Between iterations of communication_cascade_run_recurrent, if the
     * accumulated cerebellar prediction error exceeds cerebellar_pe_threshold,
     * cerebellar_correction_pending is set so the NEXT iteration's
     * motor + prosody stages know to apply the prediction's bias
     * contribution at cerebellar_correction_strength.
     *
     * Lifetime: enabled flag + strength + threshold persist for the
     * life of the brain. correction_pending is set/cleared by the
     * recurrent loop only — outside a recurrent run it is always false.
     * Diag counters are lifetime totals; not _Atomic because the
     * cascade is single-caller-at-a-time by contract (mirrors the
     * arcuate_feedback_* fields above).
     *
     * APPENDED at end of brain_struct — ABI append-only. */
    bool     cerebellar_correction_enabled;       /* master gate (default false) */
    float    cerebellar_correction_strength;      /* [0,1] — how strongly to apply
                                                    the cerebellar prediction bias
                                                    when correction_pending. */
    bool     cerebellar_correction_pending;       /* set between recurrent iters
                                                    when accumulated PE > threshold;
                                                    consumed by next iter's stages. */
    float    cerebellar_pe_threshold;             /* PE-norm gate (default 0.20). */
    /* Lifetime diagnostics surfaced via nimcp_brain_get_cerebellar_diag. */
    uint64_t cerebellar_predictions_made;
    /* S7-H3 fix (2026-05-19): cerebellar_corrections_applied counts STAGES
     * that ran while correction_pending was set — bumped inside both
     * stage_motor and stage_prosody. A single recurrent iter with both
     * stages firing increments by 2, so consumers reading this as
     * "iters with correction" get a 2x inflated count. Keep the field as
     * the STAGES counter (renamed in the diag struct field below); add a
     * separate iters counter bumped once per recurrent iter from the
     * recurrent loop tail. */
    uint64_t cerebellar_corrections_applied;       /* stages: bumped per stage */
    uint64_t cerebellar_correction_iters_applied;  /* iters: bumped per recurrent iter */
    float    cerebellar_last_pe_norm;             /* most-recent stage's PE norm */

    /* === S2-C2/C3 ARCUATE FEEDBACK CROSS-THREAD LOCK ===
     * Protects the arcuate_feedback_{vec,dim,strength} tuple above.
     * Before this lock, the recurrent loop in communication_cascade_run_recurrent
     * wrote/freed brain->arcuate_feedback_vec while a concurrent
     * cascade_stage_content read the pointer + dereferenced it — a UAF
     * was reachable if another thread invoked produce_cascade mid-recurrent
     * run (the cascade was documented as "single-caller-at-a-time" but
     * nothing enforced that contract).
     *
     * Lock-ordering: the cascade rule is that this lock is acquired only
     * around the PUBLISH (writer) + READ-DEREF (reader) of the arcuate
     * tuple — never held across other cascade-stage work. It MUST NOT
     * nest under bridge / broca / grounded_lang mutexes (see
     * docs/claude/modules/lock-ordering.md). Lazy-init via
     * cascade_arcuate_lock_ensure() — the brain struct is calloc'd so a
     * NULL pointer here means "not yet initialized".
     *
     * APPENDED at end of brain_struct — ABI append-only. */
    void*    arcuate_feedback_lock;               /* nimcp_mutex_t*, lazy-init */

    /* === SLICE D — CASCADE SELF-TRAIN EXTERNAL-REWARD GATING (2026-05-19) ===
     * Option-1 architectural rebuild: cascade_stage_self_train must NOT fire
     * unconditionally on its own production (autoconfirm loop). It gates on
     * an external reward signal — set by the caregiver-critic / RL pipeline
     * via nimcp_brain_set_last_external_reward() + the
     * set_last_external_reward RPC. */
    float    last_external_reward;
    uint64_t last_external_reward_us;

    _Atomic uint64_t cascade_self_train_skipped_stale;
    _Atomic uint64_t cascade_self_train_skipped_below_threshold;
    _Atomic uint64_t cascade_self_train_fired;

    float    cascade_self_train_reward_threshold;
    uint64_t cascade_self_train_reward_ttl_us;

    /* === SLICE E — STAGE-ANCHORED DEVELOPMENTAL SCAFFOLDING (2026-05-19) === */
    uint32_t current_stage;

    /* === SLICE B — CROSS-MODAL CONCEPT REGISTRY (2026-05-19) ===
     * Opaque pointer (concept_registry_t*). Maps (text label, visual digest,
     * audio digest) → canonical concept_pop_id. Created in Wave 27
     * (cognitive_engines). Serialized as a `.concept_registry` sidecar. */
    void*    concept_registry;

    /* === TOXICITY CLASSIFIER (2026-05-20) ===
     * Opaque pointer (toxicity_classifier_t*). Pattern-based content
     * classifier that populates predicted_harm + fairness_violation scalars
     * on the ethics action_context, drives LGSS rule evaluation, and emits
     * KG events on detection. POLICY: mark, never delete — toxic training
     * data is NEVER filtered out, only labeled. Created in the safety init
     * wave alongside LGSS. NULL when the rules file is unreadable (the
     * classifier becomes a no-op; downstream gates fall back to legacy
     * proxy scoring). */
    void*    toxicity_classifier;

    /* === TOXICITY RESPONSE ENGINE (2026-05-21, Phase 3a) ===
     * Opaque pointer (toxicity_response_t*). Generates Athena's
     * stage-appropriate counterclaim against detected toxic content. The
     * counterclaim is the operational expression of the core ethics
     * directive ("improve the human condition") — not a separate rule.
     * Loaded from data/safety/toxicity_counterclaims.tsv +
     * data/safety/toxicity_antiframes.tsv. NULL means engine creation
     * failed; gates fall back to a default refusal string. */
    void*    toxicity_response;

    /* === TOXICITY ML HEAD (2026-05-21, Task 134) ===
     * Opaque pointer (toxicity_ml_classifier_t*). Online-trained MLP that
     * ensembles with the pattern classifier via max(). NULL until the
     * head is attached. Optional — system works without it. */
    void*    toxicity_ml;

    /* === TIER 1 STEP E: CASCADE REASONING CACHE (2026-05-24) ===
     * The reasoning engine has no cheap "read last conclusion" path — only
     * a full O(100s-1000s µs) reasoning_engine_reason() invoke. So the
     * production cascade primes a conclusion vector ONCE per unique prompt
     * (at the top of communication_cascade_run) and cascade_stage_content
     * reads it cheaply on every iteration. Append-only fields (ABI-safe).
     *
     * cascade_reason_in_content gates the whole feature (default OFF —
     * opt-in, no behavior change until enabled). cascade_reasoning_vec is
     * lazily allocated at semantic_dim and reused (never realloc'd smaller),
     * so the cross-thread read in the RPC pool is UAF-free; cascade_reasoning_dim
     * == 0 means "no valid cached conclusion" → stage_content no-ops.
     * prompt_hash dedups recurrent re-entry; in_progress is a re-entrancy
     * guard against reasoning recursively triggering another respond(). */
    bool      cascade_reason_in_content;       /* opt-in flag, default OFF */
    bool      cascade_reasoning_in_progress;   /* re-entrancy guard */
    float*    cascade_reasoning_vec;           /* lazily alloc'd [cap] floats */
    uint32_t  cascade_reasoning_cap;           /* allocated capacity */
    uint32_t  cascade_reasoning_dim;           /* 0 = invalid; else valid len */
    float     cascade_reasoning_confidence;    /* chain overall_confidence */
    uint64_t  cascade_reasoning_prompt_hash;   /* dedup key for the prime */

    /* === Non-SNN networks → language content_intent (2026-06-05) ===========
     * Per directive: every network except the FNO feeds the SNN language
     * generator. They converge, like the cognitive sources above, on
     * content_intent (which seeds the Wernicke concept pop). Each is a
     * default-OFF, gated, soft-additive contributor so the shipped path is
     * byte-identical until a flag is flipped.
     *
     * ANN / LNN / CNN carry a real vector → blended into content_intent.
     * HNN has NO semantic vector (it is a layer-mode inside the LNN exposing
     * only energy scalars), so it instead MODULATES the produce confidence
     * floor from its energy-deviation: well-conserved (stable) dynamics →
     * more assertive; large deviation (unstable) → less assertive. Wiring it
     * as a fake content vector would just duplicate the LNN state. */
    bool      cascade_ann_in_content;          /* opt-in: ANN output → intent  */
    bool      cascade_lnn_in_content;          /* opt-in: LNN state  → intent  */
    bool      cascade_hnn_in_content;          /* opt-in: HNN energy → floor   */
    bool      cascade_cnn_in_content;          /* opt-in: CNN feats  → intent  */
};

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(((struct brain_struct*)0)->current_stage) == 4u,
    "brain_struct.current_stage: Slice E expects a 4-byte uint32");
_Static_assert(sizeof(((struct brain_struct*)0)->cascade_self_train_reward_ttl_us) == 8u,
    "brain_struct.cascade_self_train_reward_ttl_us: Slice D field size invariant");
#endif

/* Slice B append-counter — bump when adding more fields to brain_struct.
 * Slice B's field is concept_registry. */
#define NIMCP_BRAIN_STRUCT_ABI_SLICE_B_FIELDS  1u
_Static_assert(NIMCP_BRAIN_STRUCT_ABI_SLICE_B_FIELDS == 1u,
               "Slice B appended exactly 1 field (concept_registry). "
               "If you append more, bump this constant + this static-assert.");

//=============================================================================
// Strategy Pattern - Task-Specific Behaviors
//=============================================================================

/**
 * @brief Task strategy interface
 *
 * WHY: Different tasks (classification, regression) need different:
 * - Learning rates
 * - Output transformations
 * - Performance metrics
 *
 * PATTERN: Strategy pattern - encapsulates algorithm families
 */
struct task_strategy {
    brain_task_t task_type;

    /**
     * @brief Get recommended learning rate for this task
     * COMPLEXITY: O(1)
     */
    float (*get_learning_rate)(void);

    /**
     * @brief Transform raw output to task-specific format
     * COMPLEXITY: O(num_outputs)
     */
    void (*transform_output)(float* output, uint32_t size);

    /**
     * @brief Compute task-specific loss
     * COMPLEXITY: O(num_outputs)
     */
    float (*compute_loss)(const float* predicted, const float* target, uint32_t size);
};

//=============================================================================
// Health Agent Heartbeat Helper (Phase 8: System-Wide Health Integration)
//=============================================================================

/**
 * @brief Forward declaration for health agent heartbeat
 */
struct nimcp_health_agent;
extern void nimcp_health_agent_heartbeat_ex(struct nimcp_health_agent* agent,
                                             const char* operation,
                                             float progress);

/**
 * @brief Forward declaration for state manager
 */
struct nimcp_state_manager;

/**
 * @brief Send heartbeat for long-running brain operations
 *
 * WHAT: Notify health agent that brain is alive during long operations
 * WHY:  Prevent false-positive hang detection during expected long operations
 * HOW:  Check if health agent is enabled and call heartbeat_ex
 *
 * @param brain The brain instance
 * @param operation Description of current operation (e.g., "brain_destroy")
 * @param progress Progress indicator (0.0 to 1.0)
 */
static inline void brain_heartbeat(brain_t brain, const char* operation, float progress) {
    if (brain && brain->health_agent_enabled && brain->health_agent) {
        nimcp_health_agent_heartbeat_ex(brain->health_agent, operation, progress);
    }
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INTERNAL_H
