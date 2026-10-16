/**
 * @file nimcp_mesh_bootstrap.h
 * @brief Complete NIMCP System Mesh Network Bootstrap
 *
 * WHAT: Bootstraps the entire NIMCP system into a unified mesh network
 * WHY:  Single initialization point for all 2000+ NIMCP modules
 * HOW:  Category-based registration, auto-discovery, hierarchical wiring
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                         MESH BOOTSTRAP SYSTEM                                │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                    ORDERING SERVICE (Raft)                            │   │
 * │  │           Sequences all cross-channel transactions                     │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                                    │                                        │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                 MEMBERSHIP SERVICE PROVIDER                           │   │
 * │  │              BBB Gateway + Immune System Integration                   │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                                    │                                        │
 * │  ┌────────────┬────────────┬────────────┬────────────┬────────────┐        │
 * │  │   SYSTEM   │    LEFT    │   RIGHT    │ SUBCORTICAL│    GPU     │        │
 * │  │  CHANNEL   │   HEMI     │   HEMI     │  CHANNEL   │  CHANNEL   │        │
 * │  ├────────────┼────────────┼────────────┼────────────┼────────────┤        │
 * │  │ Thalamus   │ PFC-Left   │ PFC-Right  │ Amygdala   │ Cerebellum │        │
 * │  │ BBB        │ Broca's    │ Spatial    │ Hippocampus│ GPU-Accel  │        │
 * │  │ Immune     │ Reasoning  │ Creative   │ Basal Gang │ Batch Proc │        │
 * │  │ Bio-Router │ Language   │ Holistic   │ Hypothal.  │ Recovery   │        │
 * │  └────────────┴────────────┴────────────┴────────────┴────────────┘        │
 * │                                                                              │
 * │  CATEGORY INTEGRATIONS:                                                      │
 * │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │
 * │  │Cognitive│ │Sensory  │ │ Motor   │ │ Memory  │ │Security │ │Plasticity│  │
 * │  │ 150+    │ │  50+    │ │  30+    │ │  40+    │ │ 120+    │ │  100+   │   │
 * │  │ modules │ │ modules │ │ modules │ │ modules │ │ modules │ │ modules │   │
 * │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘   │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_BOOTSTRAP_H
#define NIMCP_MESH_BOOTSTRAP_H

#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_bio_integration.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations - Core Brain Systems
 * ============================================================================ */

/* Core brain structures */
typedef struct hemispheric_brain hemispheric_brain_t;
typedef struct thalamus thalamus_t;
typedef struct blood_brain_barrier blood_brain_barrier_t;
typedef struct brain_immune_system brain_immune_system_t;

/* Limbic system */
typedef struct amygdala amygdala_t;
typedef struct hippocampus hippocampus_t;
typedef struct hypothalamus hypothalamus_t;

/* Cortical areas */
typedef struct prefrontal_cortex prefrontal_cortex_t;
typedef struct motor_cortex motor_cortex_t;
typedef struct sensory_cortex sensory_cortex_t;
typedef struct visual_cortex visual_cortex_t;
typedef struct auditory_cortex auditory_cortex_t;

/* Subcortical */
typedef struct basal_ganglia basal_ganglia_t;
typedef struct cerebellum cerebellum_t;

/* Cognitive modules */
typedef struct fep_orchestrator fep_orchestrator_t;
typedef struct attention_manager attention_manager_t;
typedef struct working_memory working_memory_t;
typedef struct reasoning_engine reasoning_engine_t;
typedef struct planning_module planning_module_t;

/* Swarm/consensus */
typedef struct gossip_beliefs_context gossip_beliefs_context_t;
typedef struct swarm_consensus swarm_consensus_t;
typedef struct collective_workspace collective_workspace_t;

/* Async/routing - bio_router_t is a pointer typedef */
typedef struct bio_router_struct* bio_router_t;
typedef struct bio_scheduler bio_scheduler_t;

/* GPU modules */
typedef struct gpu_recovery_context gpu_recovery_context_t;
typedef struct gpu_batch_processor gpu_batch_processor_t;

/* Plasticity */
typedef struct plasticity_coordinator plasticity_coordinator_t;
typedef struct stdp_module stdp_module_t;

/* Glial */
typedef struct astrocyte_network astrocyte_network_t;
typedef struct microglia_system microglia_system_t;

/* ============================================================================
 * Bootstrap Configuration
 * ============================================================================ */

/**
 * @brief Subsystem enable flags
 */
typedef struct mesh_subsystem_flags {
    bool enable_cognitive;          /**< Cognitive modules (FEP, reasoning, etc.) */
    bool enable_sensory;            /**< Sensory processing modules */
    bool enable_motor;              /**< Motor control modules */
    bool enable_memory;             /**< Memory systems */
    bool enable_security;           /**< BBB + Immune integration */
    bool enable_gpu;                /**< GPU acceleration modules */
    bool enable_plasticity;         /**< Learning/plasticity modules */
    bool enable_glial;              /**< Glial support systems */
    bool enable_swarm;              /**< Swarm consensus modules */
    bool enable_async;              /**< Bio-async routing */
    bool enable_lnn;                /**< Liquid neural networks */
    bool enable_snn;                /**< Spiking neural networks */
    bool enable_nlp;                /**< NLP modules */
    bool enable_superhuman;         /**< Advanced perception modules */
    bool enable_quantum;            /**< Quantum computing modules */
} mesh_subsystem_flags_t;

/**
 * @brief Bootstrap configuration
 */
typedef struct mesh_bootstrap_config {
    /* Base integration config */
    mesh_integration_config_t integration;

    /* Subsystem enables */
    mesh_subsystem_flags_t subsystems;

    /* Core brain handles (optional - will create if NULL) */
    hemispheric_brain_t* brain;
    thalamus_t* thalamus;
    blood_brain_barrier_t* bbb;
    brain_immune_system_t* immune;

    /* Auto-discovery */
    bool auto_discover_modules;     /**< Scan for available modules */
    bool auto_register_all;         /**< Register all discovered modules */

    /* Health monitoring */
    bool enable_health_monitoring;
    float health_check_interval_ms;

    /* Logging */
    bool verbose_logging;

} mesh_bootstrap_config_t;

/**
 * @brief Bootstrap statistics
 */
typedef struct mesh_bootstrap_stats {
    /* Registration counts by category */
    size_t cognitive_modules;
    size_t sensory_modules;
    size_t motor_modules;
    size_t memory_modules;
    size_t security_modules;
    size_t gpu_modules;
    size_t plasticity_modules;
    size_t glial_modules;
    size_t swarm_modules;
    size_t async_modules;
    size_t lnn_modules;
    size_t snn_modules;
    size_t nlp_modules;
    size_t superhuman_modules;
    size_t quantum_modules;

    /* Totals */
    size_t total_modules_registered;
    size_t total_channels_active;
    size_t total_policies_created;
    size_t total_endorsers_registered;

    /* Status */
    bool fully_initialized;
    uint64_t initialization_time_ns;

} mesh_bootstrap_stats_t;

/* ============================================================================
 * Bootstrap Handle
 * ============================================================================ */

/**
 * @brief Mesh bootstrap handle (opaque)
 */
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bootstrap configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_default_config(mesh_bootstrap_config_t* config);

/**
 * @brief Create mesh bootstrap with all subsystems
 *
 * WHAT: Initialize the complete NIMCP mesh network
 * WHY:  Single entry point for full system initialization
 *
 * @param config Bootstrap configuration (NULL for defaults)
 * @return Bootstrap handle or NULL on failure
 */
mesh_bootstrap_t* mesh_bootstrap_create(const mesh_bootstrap_config_t* config);

/**
 * @brief Destroy mesh bootstrap and cleanup all registrations
 *
 * @param bootstrap Bootstrap to destroy (NULL-safe)
 */
void mesh_bootstrap_destroy(mesh_bootstrap_t* bootstrap);

/* ============================================================================
 * Component Access
 * ============================================================================ */

/**
 * @brief Get mesh integration handle
 *
 * @param bootstrap Bootstrap handle
 * @return Integration handle or NULL
 */
mesh_integration_t* mesh_bootstrap_get_integration(mesh_bootstrap_t* bootstrap);

/**
 * @brief Get participant registry
 *
 * @param bootstrap Bootstrap handle
 * @return Registry or NULL
 */
mesh_participant_registry_t* mesh_bootstrap_get_registry(mesh_bootstrap_t* bootstrap);

/**
 * @brief Get channel by ID
 *
 * @param bootstrap Bootstrap handle
 * @param channel_id Channel ID
 * @return Channel or NULL
 */
mesh_channel_t* mesh_bootstrap_get_channel(
    mesh_bootstrap_t* bootstrap,
    mesh_channel_id_t channel_id
);

/* ============================================================================
 * Manual Registration API
 * ============================================================================ */

/**
 * @brief Register hemispheric brain
 *
 * @param bootstrap Bootstrap handle
 * @param brain Hemispheric brain instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_brain(
    mesh_bootstrap_t* bootstrap,
    hemispheric_brain_t* brain
);

/**
 * @brief Register thalamus as central gateway
 *
 * @param bootstrap Bootstrap handle
 * @param thalamus Thalamus instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_thalamus(
    mesh_bootstrap_t* bootstrap,
    thalamus_t* thalamus
);

/**
 * @brief Register BBB for security gateway
 *
 * @param bootstrap Bootstrap handle
 * @param bbb Blood-brain barrier instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_bbb(
    mesh_bootstrap_t* bootstrap,
    blood_brain_barrier_t* bbb
);

/**
 * @brief Register immune system
 *
 * @param bootstrap Bootstrap handle
 * @param immune Brain immune system instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_immune(
    mesh_bootstrap_t* bootstrap,
    brain_immune_system_t* immune
);

/**
 * @brief Register amygdala with VETO role
 *
 * @param bootstrap Bootstrap handle
 * @param amygdala Amygdala instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_amygdala(
    mesh_bootstrap_t* bootstrap,
    amygdala_t* amygdala
);

/**
 * @brief Register hippocampus for memory
 *
 * @param bootstrap Bootstrap handle
 * @param hippocampus Hippocampus instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_hippocampus(
    mesh_bootstrap_t* bootstrap,
    hippocampus_t* hippocampus
);

/**
 * @brief Register FEP orchestrator
 *
 * @param bootstrap Bootstrap handle
 * @param fep FEP orchestrator instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_fep(
    mesh_bootstrap_t* bootstrap,
    fep_orchestrator_t* fep
);

/**
 * @brief Register bio-router for message routing
 *
 * @param bootstrap Bootstrap handle
 * @param router Bio-router instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_bio_router(
    mesh_bootstrap_t* bootstrap,
    bio_router_t* router
);

/**
 * @brief Register GPU recovery context
 *
 * @param bootstrap Bootstrap handle
 * @param gpu_recovery GPU recovery context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_gpu_recovery(
    mesh_bootstrap_t* bootstrap,
    gpu_recovery_context_t* gpu_recovery
);

/**
 * @brief Register plasticity coordinator
 *
 * @param bootstrap Bootstrap handle
 * @param plasticity Plasticity coordinator
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_plasticity(
    mesh_bootstrap_t* bootstrap,
    plasticity_coordinator_t* plasticity
);

/**
 * @brief Register gossip beliefs context
 *
 * @param bootstrap Bootstrap handle
 * @param gossip Gossip context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_gossip(
    mesh_bootstrap_t* bootstrap,
    gossip_beliefs_context_t* gossip
);

/**
 * @brief Register swarm consensus
 *
 * @param bootstrap Bootstrap handle
 * @param swarm Swarm consensus
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_swarm(
    mesh_bootstrap_t* bootstrap,
    swarm_consensus_t* swarm
);

/* ============================================================================
 * Category Registration API
 * ============================================================================ */

/**
 * @brief Register all cognitive modules
 *
 * Registers: FEP, attention, working memory, reasoning, planning,
 *            inner dialogue, metacognition, executive function, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_cognitive_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all sensory modules
 *
 * Registers: Visual, auditory, somatosensory, olfactory, gustatory,
 *            vestibular, proprioception, cross-modal integration, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_sensory_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all motor modules
 *
 * Registers: Motor cortex, premotor, supplementary motor, cerebellum,
 *            basal ganglia, motor planning, motor execution, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_motor_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all memory modules
 *
 * Registers: Hippocampus, working memory, episodic memory, semantic memory,
 *            procedural memory, memory consolidation, recall, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_memory_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all security modules
 *
 * Registers: BBB, immune system, threat detection, antigen processing,
 *            antibody generation, inflammation, quarantine, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_security_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all GPU modules
 *
 * Registers: GPU recovery, batch processing, multi-GPU coordination,
 *            CUDA kernels, memory management, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_gpu_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all plasticity modules
 *
 * Registers: STDP, LTP, LTD, homeostatic plasticity, metaplasticity,
 *            structural plasticity, neuromodulation, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_plasticity_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all glial modules
 *
 * Registers: Astrocytes, microglia, oligodendrocytes, glial networks,
 *            metabolic support, waste clearance, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_glial_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all swarm modules
 *
 * Registers: Gossip beliefs, swarm consensus, collective workspace,
 *            FEP convergence, distributed coordination, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_swarm_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all async modules
 *
 * Registers: Bio-router, bio-scheduler, bio-promise, bio-future,
 *            neuromodulator channels, phase synchronization, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_async_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all LNN modules
 *
 * Registers: Liquid neural networks, LNN clusters, LNN bridges,
 *            continuous-time neural networks, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_lnn_modules(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register all SNN modules
 *
 * Registers: Spiking neural networks, spike timing, spike propagation,
 *            STDP-based learning, neuromorphic computing, etc.
 *
 * @param bootstrap Bootstrap handle
 * @return Number of modules registered
 */
size_t mesh_bootstrap_register_snn_modules(mesh_bootstrap_t* bootstrap);

/* ============================================================================
 * Update and Processing
 * ============================================================================ */

/**
 * @brief Update all mesh components
 *
 * @param bootstrap Bootstrap handle
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_update(
    mesh_bootstrap_t* bootstrap,
    uint64_t delta_ms
);

/**
 * @brief Process pending transactions across all channels
 *
 * @param bootstrap Bootstrap handle
 * @return Number of transactions processed
 */
size_t mesh_bootstrap_process_transactions(mesh_bootstrap_t* bootstrap);

/**
 * @brief Run gossip rounds on all channels
 *
 * @param bootstrap Bootstrap handle
 * @param rounds Number of gossip rounds
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_gossip_all(
    mesh_bootstrap_t* bootstrap,
    size_t rounds
);

/**
 * @brief Check if system has converged
 *
 * @param bootstrap Bootstrap handle
 * @return true if all channels converged
 */
bool mesh_bootstrap_has_converged(const mesh_bootstrap_t* bootstrap);

/**
 * @brief Get system-wide free energy
 *
 * @param bootstrap Bootstrap handle
 * @return Average free energy across all channels
 */
float mesh_bootstrap_get_free_energy(const mesh_bootstrap_t* bootstrap);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get bootstrap statistics
 *
 * @param bootstrap Bootstrap handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_get_stats(
    const mesh_bootstrap_t* bootstrap,
    mesh_bootstrap_stats_t* stats
);

/**
 * @brief Print bootstrap status
 *
 * @param bootstrap Bootstrap handle
 */
void mesh_bootstrap_print_status(const mesh_bootstrap_t* bootstrap);

/* ============================================================================
 * Pattern-Based Routing API
 * ============================================================================ */

/**
 * @brief Get the pattern router for brain-like self-selection
 *
 * The pattern router enables modules to participate based on pattern
 * similarity rather than predefined transaction types. This is how
 * the brain routes - no central "type checker", modules activate
 * based on pattern match.
 *
 * @param bootstrap Bootstrap handle
 * @return Pattern router or NULL
 */
mesh_pattern_router_t* mesh_bootstrap_get_pattern_router(mesh_bootstrap_t* bootstrap);

/* ============================================================================
 * Phase 14 Bridge Access API
 * ============================================================================ */

/**
 * @brief Get the module registry for type-safe module lookup
 *
 * The module registry replaces dummy pointer registration with real
 * module instances. Modules register with magic number validation
 * for type safety.
 *
 * @param bootstrap Bootstrap handle
 * @return Module registry or NULL
 */
mesh_module_registry_t* mesh_bootstrap_get_module_registry(mesh_bootstrap_t* bootstrap);

/**
 * @brief Get the bio bridge for bio-async to mesh translation
 *
 * The bio bridge translates bio-router messages to mesh transactions
 * and vice versa. This enables seamless integration between the
 * bio-async routing system and the mesh network.
 *
 * @param bootstrap Bootstrap handle
 * @return Bio bridge or NULL
 */
mesh_bio_bridge_t* mesh_bootstrap_get_bio_bridge(mesh_bootstrap_t* bootstrap);

/**
 * @brief Get the exception bridge for immune system integration
 *
 * The exception bridge routes exceptions through the mesh to the
 * immune system for coordinated response. Exceptions are converted
 * to antigens and validated through the BBB.
 *
 * @param bootstrap Bootstrap handle
 * @return Exception bridge or NULL
 */
mesh_exception_bridge_t* mesh_bootstrap_get_exception_bridge(mesh_bootstrap_t* bootstrap);

/**
 * @brief Get the health bridge for distributed health monitoring
 *
 * The health bridge aggregates health agent heartbeats through
 * the mesh network, providing system-wide health status and
 * enabling consensus-based health decisions.
 *
 * @param bootstrap Bootstrap handle
 * @return Health bridge or NULL
 */
mesh_health_bridge_t* mesh_bootstrap_get_health_bridge(mesh_bootstrap_t* bootstrap);

/**
 * @brief Get the bio-mesh integration for router hookup
 *
 * The bio integration connects the bio-async router to the mesh network,
 * enabling consensus-based message routing. When connected, messages can
 * optionally be routed through mesh channels for distributed processing.
 *
 * @param bootstrap Bootstrap handle
 * @return Bio integration or NULL
 */
struct mesh_bio_integration* mesh_bootstrap_get_bio_integration(mesh_bootstrap_t* bootstrap);

/**
 * @brief Register a module with type-safe validation
 *
 * This is the preferred way to register modules in Phase 14+.
 * Uses the module registry for type-safe lookup with magic validation.
 *
 * @param bootstrap Bootstrap handle
 * @param descriptor Module descriptor with type info
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bootstrap_register_module(
    mesh_bootstrap_t* bootstrap,
    const mesh_module_descriptor_t* descriptor
);

/**
 * @brief Lookup a registered module by name
 *
 * @param bootstrap Bootstrap handle
 * @param name Module name
 * @return Registered module or NULL if not found
 */
mesh_registered_module_t* mesh_bootstrap_lookup_module(
    mesh_bootstrap_t* bootstrap,
    const char* name
);

/**
 * @brief Register a module's receptive field for pattern-based routing
 *
 * After registering as a participant, modules can also register their
 * "receptive field" - the patterns they respond to. This enables
 * brain-like self-selection where modules activate based on pattern
 * similarity rather than explicit transaction type matching.
 *
 * @param bootstrap Bootstrap handle
 * @param module_id Participant ID of the module
 * @param field Receptive field defining what patterns the module responds to
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t mesh_bootstrap_register_receptive_field(
    mesh_bootstrap_t* bootstrap,
    mesh_participant_id_t module_id,
    const mesh_receptive_field_t* field
);

/**
 * @brief Route a pattern-based transaction (brain-like self-selection)
 *
 * Instead of using predefined transaction types, route based on pattern
 * similarity. Modules with receptive fields matching the transaction
 * pattern will self-select as potential endorsers.
 *
 * @param bootstrap Bootstrap handle
 * @param tx Pattern-based transaction
 * @param endorsers Output array for selected endorsers
 * @param max_endorsers Maximum number of endorsers
 * @param count_out Output: actual number of endorsers selected
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t mesh_bootstrap_route_by_pattern(
    mesh_bootstrap_t* bootstrap,
    const mesh_pattern_transaction_t* tx,
    mesh_participant_id_t* endorsers,
    size_t max_endorsers,
    size_t* count_out
);

/**
 * @brief Apply neuromodulation to pattern routing
 *
 * BRAIN ANALOGY:
 *   Dopamine  → Increases salience of reward-related patterns
 *   Norepinephrine → Increases urgency, broadens receptive fields
 *   Acetylcholine → Sharpens attention, narrows focus
 *   Serotonin → Modulates emotional/social patterns
 *
 * @param bootstrap Bootstrap handle
 * @param neuromod Neuromodulator type
 * @param level Modulation level [0.0, 1.0]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t mesh_bootstrap_apply_neuromodulation(
    mesh_bootstrap_t* bootstrap,
    mesh_neuromodulator_t neuromod,
    float level
);

/**
 * @brief Learn from pattern routing outcome
 *
 * After a transaction completes, teach the pattern router whether
 * the endorser selection was good or bad. This enables the system
 * to learn which modules should participate together.
 *
 * @param bootstrap Bootstrap handle
 * @param tx The transaction that was processed
 * @param endorsers Modules that endorsed
 * @param endorser_count Number of endorsers
 * @param success Whether the transaction succeeded
 * @param reward_signal Reward signal [-1.0, 1.0]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t mesh_bootstrap_learn_routing_outcome(
    mesh_bootstrap_t* bootstrap,
    const mesh_pattern_transaction_t* tx,
    const mesh_participant_id_t* endorsers,
    size_t endorser_count,
    bool success,
    float reward_signal
);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * @brief Initialize mesh with all default subsystems enabled
 */
#define MESH_BOOTSTRAP_ALL() mesh_bootstrap_create(NULL)

/**
 * @brief Initialize mesh with specific subsystems
 */
#define MESH_BOOTSTRAP_WITH(flags) \
    ({ \
        mesh_bootstrap_config_t _cfg; \
        mesh_bootstrap_default_config(&_cfg); \
        _cfg.subsystems = (flags); \
        mesh_bootstrap_create(&_cfg); \
    })

/**
 * @brief Default subsystem flags - all enabled
 */
#define MESH_SUBSYSTEMS_ALL ((mesh_subsystem_flags_t){ \
    .enable_cognitive = true, \
    .enable_sensory = true, \
    .enable_motor = true, \
    .enable_memory = true, \
    .enable_security = true, \
    .enable_gpu = true, \
    .enable_plasticity = true, \
    .enable_glial = true, \
    .enable_swarm = true, \
    .enable_async = true, \
    .enable_lnn = true, \
    .enable_snn = true, \
    .enable_nlp = true, \
    .enable_superhuman = false, \
    .enable_quantum = false \
})

/**
 * @brief Minimal subsystem flags - core only
 */
#define MESH_SUBSYSTEMS_CORE ((mesh_subsystem_flags_t){ \
    .enable_cognitive = true, \
    .enable_sensory = true, \
    .enable_motor = true, \
    .enable_memory = true, \
    .enable_security = true, \
    .enable_gpu = false, \
    .enable_plasticity = false, \
    .enable_glial = false, \
    .enable_swarm = true, \
    .enable_async = true, \
    .enable_lnn = false, \
    .enable_snn = false, \
    .enable_nlp = false, \
    .enable_superhuman = false, \
    .enable_quantum = false \
})

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_BOOTSTRAP_H */
