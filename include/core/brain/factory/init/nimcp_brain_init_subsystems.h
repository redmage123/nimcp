//=============================================================================
// nimcp_brain_init_subsystems.h - Brain Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_subsystems.h
 * @brief Brain subsystem initialization functions
 *
 * WHAT: All brain subsystem initialization functions (37 functions)
 * WHY:  Separates subsystem initialization from core brain creation
 * HOW:  Each function initializes a specific brain subsystem
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_BRAIN_INIT_SUBSYSTEMS_H
#define NIMCP_BRAIN_INIT_SUBSYSTEMS_H

#include "core/brain/nimcp_brain.h"
#include "cognitive/mental_health/nimcp_mental_health_guardian.h"

#ifdef __cplusplus
extern "C" {
#endif

// Glial and biological subsystems
bool nimcp_brain_factory_init_glial_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_glial_subsystem(brain_t brain);
void nimcp_brain_attach_glial(brain_t brain);

// Glial accessors (opaque pointer returns; tests and diagnostics).
// The underlying types are defined in include/glial/{integration,astrocytes,
// oligodendrocytes,microglia}/*.h. Tests that need field access include
// those headers directly and cast the void* back.
void* nimcp_brain_get_glial(brain_t brain);
void* nimcp_brain_get_astrocyte_network(brain_t brain);
void* nimcp_brain_get_oligodendrocyte_network(brain_t brain);
void* nimcp_brain_get_microglia_network(brain_t brain);
bool  nimcp_brain_glial_is_enabled(brain_t brain);
bool nimcp_brain_factory_init_multimodal_subsystems(brain_t brain);
bool nimcp_brain_factory_init_pink_noise_subsystem(brain_t brain);
bool nimcp_brain_factory_init_neuromodulator_system(brain_t brain);
bool nimcp_brain_factory_init_spatial_neuromod_system(brain_t brain);
bool nimcp_brain_factory_init_neuromod_nuclei(brain_t brain);  // Phase 4: LC, VTA, Raphe, Habenula
bool nimcp_brain_factory_init_attention_subsystem(brain_t brain);
bool nimcp_brain_factory_init_brain_regions_subsystem(brain_t brain);

// Cognitive subsystems
bool nimcp_brain_factory_init_symbolic_logic_subsystem(brain_t brain);
bool nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain_t brain);
bool nimcp_brain_factory_init_epistemic_subsystem(brain_t brain);
bool nimcp_brain_factory_init_working_memory_subsystem(brain_t brain);
bool nimcp_brain_factory_init_executive_subsystem(brain_t brain);
bool nimcp_brain_factory_init_theory_of_mind_subsystem(brain_t brain);
bool nimcp_brain_factory_init_natural_explanations_subsystem(brain_t brain);
bool nimcp_brain_factory_init_meta_learning_subsystem(brain_t brain);
bool nimcp_brain_factory_init_mental_health_subsystem(brain_t brain);
bool nimcp_brain_factory_init_trauma_resilience(brain_t brain);
bool nimcp_brain_factory_init_predictive_subsystem(brain_t brain);
bool nimcp_brain_factory_init_mirror_neurons(brain_t brain);
bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t brain);
bool nimcp_brain_factory_init_autobiographical_memory_subsystem(brain_t brain);
bool nimcp_brain_factory_init_self_model_subsystem(brain_t brain);

// Plasticity subsystems
bool nimcp_brain_factory_init_homeostatic_plasticity_subsystem(brain_t brain);
bool nimcp_brain_factory_init_dendritic_computation_subsystem(brain_t brain);
bool nimcp_brain_factory_init_biological_predictive_subsystem(brain_t brain);

// === PLASTICITY BRIDGES (Phase 7: Cognitive Substrate Integration) ===
// These bridges connect plasticity mechanisms with higher-level cognitive systems:
// - STDP-Omni: Bidirectional STDP ↔ Omnidirectional inference
// - STDP-PR: Bidirectional STDP ↔ Prime Resonant memory
// - Eligibility-PR: Bidirectional Eligibility traces ↔ PR memory
// - STDP-Quantum: Quantum-inspired STDP learning rate optimization
bool nimcp_brain_factory_init_stdp_omni_bridge_subsystem(brain_t brain);
bool nimcp_brain_factory_init_stdp_pr_bridge_subsystem(brain_t brain);
bool nimcp_brain_factory_init_eligibility_pr_bridge_subsystem(brain_t brain);
bool nimcp_brain_factory_init_stdp_quantum_bridge_subsystem(brain_t brain);

// === PHASE 6 SENSORY MODULES (BR-9/10/11) ===
// Three remaining sensory modalities: touch, smell, taste
// Each provides sensory-specific processing with full NIMCP integration
bool nimcp_brain_factory_init_somatosensory_subsystem(brain_t brain);  // BR-9: Touch, proprioception, pain
bool nimcp_brain_factory_init_olfactory_subsystem(brain_t brain);      // BR-10: Smell processing
bool nimcp_brain_factory_init_gustatory_subsystem(brain_t brain);      // BR-11: Taste processing

// Training subsystem
bool nimcp_brain_factory_init_training_subsystem(brain_t brain);

// Higher-order cognitive functions
bool nimcp_brain_factory_init_consolidation_subsystem(brain_t brain);
bool nimcp_brain_factory_init_curiosity_subsystem(brain_t brain);
bool nimcp_brain_factory_init_salience_subsystem(brain_t brain);
bool nimcp_brain_factory_init_introspection_subsystem(brain_t brain);
bool nimcp_brain_factory_init_connectivity_health_subsystem(brain_t brain);
bool nimcp_brain_factory_init_middleware_controller_subsystem(brain_t brain);
bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t brain);
bool nimcp_brain_factory_init_empathy_network_subsystem(brain_t brain);
bool nimcp_brain_factory_init_empathetic_response_subsystem(brain_t brain);

// Structural subsystems
bool nimcp_brain_factory_init_axon_subsystem(brain_t brain);
bool nimcp_brain_factory_init_dendrite_subsystem(brain_t brain);
bool nimcp_brain_factory_init_cortical_columns_subsystem(brain_t brain);

// GPU Neural Substrate (unified GPU acceleration for axons, dendrites, myelin, glial, etc.)
bool nimcp_brain_factory_init_substrate_gpu_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_substrate_gpu_subsystem(brain_t brain);

// CPU Neural Substrate + Thalamic Router (Phase 1-4 biological adapters for SNN/LNN/CNN)
// See src/core/brain/factory/init/nimcp_brain_init_substrate_thalamic.c
bool nimcp_brain_factory_init_substrate_thalamic_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_substrate_thalamic_subsystem(brain_t brain);
void nimcp_brain_attach_substrate_thalamic(brain_t brain);

// Accessor helpers (opaque pointers — callers cast as needed without needing
// the full brain_internal.h, which has GPU-header conflicts under C++).
struct neural_substrate;
struct thalamic_router;
struct snn_network_s;
struct lnn_network_s;
struct neural_substrate* nimcp_brain_get_substrate(brain_t brain);
struct thalamic_router*  nimcp_brain_get_thalamic_router(brain_t brain);
bool nimcp_brain_substrate_is_enabled(brain_t brain);
bool nimcp_brain_thalamic_router_is_enabled(brain_t brain);
// Returns non-NULL if network is attached AND its substrate pointer matches
// brain->substrate. Used by wire-up regression tests.
struct neural_substrate* nimcp_brain_snn_get_substrate_ref(brain_t brain);
struct neural_substrate* nimcp_brain_lnn_get_substrate_ref(brain_t brain);
// Returns pointer to LNN's thalamic_channel_s (opaque here) or NULL.
void* nimcp_brain_lnn_get_thalamic_channel_ref(brain_t brain);

// FEP Orchestrator (central coordination of all FEP bridges)
bool nimcp_brain_factory_init_fep_orchestrator_subsystem(brain_t brain);

// Core Directives (ethical foundation - Asimov's Laws, Golden Rule, Harm Prevention)
bool nimcp_brain_factory_init_core_directives_subsystem(brain_t brain);

// === COORDINATOR/ORCHESTRATOR SUBSYSTEMS ===
// These provide system-wide coordination across NIMCP
// Initialization order matters due to dependencies:
// 1. Bio-Async Orchestrator (foundation for inter-module messaging)
// 2. Plasticity Coordinator (depends on bio-async)
// 3. Immune Bridge Coordinator (depends on bio-async, brain immune)
// 4. Cognitive Meta-Controller (depends on plasticity, working memory, executive)
// 5. Security-Perception Bridge (depends on BBB, immune, perception cortices)
// 6. Swarm Module Registry (depends on all above, swarm_brain)

bool nimcp_brain_factory_init_bio_async_orchestrator_subsystem(brain_t brain);
bool nimcp_brain_factory_init_plasticity_coordinator_subsystem(brain_t brain);
bool nimcp_brain_factory_init_immune_bridge_coordinator_subsystem(brain_t brain);
bool nimcp_brain_factory_init_cognitive_meta_controller_subsystem(brain_t brain);
bool nimcp_brain_factory_init_security_perception_bridge_subsystem(brain_t brain);
bool nimcp_brain_factory_init_swarm_module_registry_subsystem(brain_t brain);

// === MEDULLA OBLONGATA SUBSYSTEM ===
// Brainstem autonomic regulation (arousal, protection, circadian, coupling)
// Must be initialized early - provides foundational regulation
bool nimcp_brain_factory_init_medulla_subsystem(brain_t brain);

// === HYPOTHALAMUS SUBSYSTEM (Homeostatic Regulation) ===
// Master regulator of homeostasis: temperature, hunger, thirst, circadian, HPA axis
// Depends on: Medulla (for arousal input), Connects to: Emotional, Sleep, Immune
bool nimcp_brain_factory_init_hypothalamus_subsystem(brain_t brain);

// === PARIETAL LOBE SUBSYSTEM (Phase 7.2) ===
// Mathematical/Scientific reasoning (number sense, spatial, equations)
// Provides quantitative and scientific cognition capabilities
bool nimcp_brain_factory_init_parietal_subsystem(brain_t brain);

// === INTUITION SYSTEM SUBSYSTEM (Phase 6: Creative/Intuitive Reasoning) ===
// Integration of all 7 Phase 6 reasoning engines:
// Intuitive, Analogical, Insight, Hypothesis, Blending, Counterfactual, Meta
// Provides higher-order cognition through intuitive leaps and creative reasoning
bool nimcp_brain_factory_init_intuition_subsystem(brain_t brain);

// === DRAGONFLY SUBSYSTEM (Bio-inspired Target Tracking) ===
// Target tracking and interception (TSDN, CSTMD1, prediction, navigation)
// Provides 95% success rate bio-inspired hunting capabilities
bool nimcp_brain_factory_init_dragonfly_subsystem(brain_t brain);

// === KNOWLEDGE GRAPH READER (Self-Awareness) ===
// Runtime access to NIMCP's self-knowledge stored in .aim/memory-nimcp.jsonl
// Enables structural introspection: "What modules do I have?" "How am I organized?"
bool nimcp_brain_factory_init_kg_reader_subsystem(brain_t brain);

// === INTERNAL KNOWLEDGE GRAPH (Runtime Module Mapping) ===
// In-memory CRUD graph for dynamic module topology with security integration
// Features: token-based access control, integrity checks, immune system integration
// Enables real-time self-awareness and adaptive behavior based on module state
bool nimcp_brain_factory_init_internal_kg_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_internal_kg_subsystem(brain_t brain);

// === MENTAL HEALTH GUARDIAN (Independent Monitoring Agent) ===
// Background agent that monitors brain mental health in real-time
// Detects disorders, applies graduated interventions (OBSERVE → QUARANTINE)
// Integrates with immune system for threat reporting and internal KG for topology
bool nimcp_brain_factory_init_mental_health_guardian_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_mental_health_guardian_subsystem(brain_t brain);

// Brain accessor functions for mental health guardian
mental_health_guardian_t* brain_get_mental_health_guardian(brain_t brain);
bool brain_start_mental_health_guardian(brain_t brain);
bool brain_stop_mental_health_guardian(brain_t brain);
bool brain_get_mental_health_guardian_status(brain_t brain, mental_health_guardian_status_t* status);

// === FAULT TOLERANCE SUBSYSTEM (Intelligent Recovery) ===
// Recovery executive with parietal integration for intelligent code repair
// Uses software engineering analysis, pattern detection, and spatial reasoning
bool nimcp_brain_factory_init_fault_tolerance_subsystem(brain_t brain);

// === HEALTH AGENT SUBSYSTEM (Autonomous Monitoring) ===
// Independent monitoring thread that watches brain health continuously
// Integrates with: memory, SNN/LNN, Dragonfly/Portia, oscillations, immune
bool nimcp_brain_factory_init_health_agent_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_health_agent_subsystem(brain_t brain);

// === CYCLE COORDINATOR SUBSYSTEM (Unified Cycle Observability) ===
// Monitors all 9 brain cycle types with health tracking, stall detection,
// dependency management, and bidirectional integration with immune, bio-async,
// KG, introspection, hemispheric, FEP, meta-learning, pink noise, global workspace,
// attention, and world model subsystems
bool nimcp_brain_factory_init_cycle_coordinator_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_cycle_coordinator_subsystem(brain_t brain);
struct brain_cycle_coordinator* brain_get_cycle_coordinator(brain_t brain);

// === FUZZY LOGIC SUBSYSTEM ===
// Cross-cutting fuzzy logic utility providing graded membership reasoning
// for risk assessment, market classification, training convergence,
// plasticity rate scheduling, and ethical decision scoring
bool nimcp_brain_factory_init_fuzzy_subsystem(brain_t brain);

// === SPINAL CORD SUBSYSTEM (Motor Output / Final Common Pathway) ===
// Motor pools, CPGs, reflex arcs, descending tract integration
// Depends on: Motor cortex, Cerebellum, Brainstem, Bio-async
bool nimcp_brain_factory_init_spinal_cord_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_spinal_cord_subsystem(brain_t brain);

// === EDGE SUBSYSTEM (Sensor Hub + Safety Watchdog + Swarm Bridges) ===
// Sensor hub, safety watchdog, Portia-Swarm bridge, Dragonfly-Swarm bridge
// Provides embodied deployment on robots, drones, and edge devices
bool nimcp_brain_factory_init_edge_subsystem(brain_t brain);

// Health agent accessor functions
struct nimcp_health_agent* brain_get_health_agent(brain_t brain);
bool brain_start_health_agent(brain_t brain);
bool brain_stop_health_agent(brain_t brain);
float brain_get_health_score(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_SUBSYSTEMS_H
