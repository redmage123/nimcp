//=============================================================================
// nimcp_brain_init_world_model.h - World Model Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_world_model.h
 * @brief Header for World Model subsystem initialization
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Brain factory initialization function for world models
 * WHY:  Integrate generative world models into brain lifecycle
 * HOW:  Called by brain_create_custom() during subsystem initialization
 *
 * MODELS:
 * - Omni World Model: Omnidirectional prediction (forward/backward/lateral)
 *   Based on DreamerV3 RSSM architecture for mental simulation
 * - Multimodal World Model: Cross-modal state prediction and sensory fusion
 *   Enables unified world representation across sensory domains
 *
 * INTEGRATION:
 * - Active Inference: Policy evaluation via world model simulation
 * - Imagination Engine: Scene generation using world model dynamics
 * - Hippocampus: Memory replay and consolidation
 * - Predictive Processing: Forward model for prediction errors
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_WORLD_MODEL_H
#define NIMCP_BRAIN_INIT_WORLD_MODEL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

struct brain_struct;

//=============================================================================
// Subsystem Initialization Functions
//=============================================================================

/**
 * @brief Initialize World Model subsystem for a brain
 *
 * WHAT: Creates and configures omni and multimodal world models
 * WHY:  Enables generative simulation, counterfactual reasoning, and policy evaluation
 * HOW:  Called by brain factory during brain creation
 *
 * Components initialized:
 * - Omni World Model: RSSM-based omnidirectional dynamics model
 *   - Forward dynamics: Predict future from current state + action
 *   - Backward dynamics: Infer past states and actions
 *   - Lateral dynamics: Cross-modal state prediction
 *   - Dreaming: Offline simulation for learning
 *
 * - Multimodal World Model: Cross-modal fusion and prediction
 *   - Sensory encoders: Visual, auditory, tactile, proprioceptive
 *   - Entity tracking: Track objects/agents across modalities
 *   - Fusion: Hierarchical or attention-based cross-modal fusion
 *
 * COMPLEXITY: O(1) for initialization
 *
 * @param brain Brain structure to initialize world model for
 * @return true on success, false on failure
 *
 * @note Called by brain_create_custom() if config.enable_world_model is true
 * @note Respects config.lazy_world_model_init flag
 */
bool nimcp_brain_factory_init_world_model_subsystem(struct brain_struct* brain);

/**
 * @brief Destroy World Model subsystem for a brain
 *
 * WHAT: Releases all world model resources
 * WHY:  Cleanup during brain destruction
 * HOW:  Called by brain_destroy()
 *
 * @param brain Brain structure to cleanup world model for
 *
 * @note Called by brain_destroy() during brain cleanup
 * @note Safe to call on uninitialized or already-destroyed brain
 */
void nimcp_brain_factory_destroy_world_model_subsystem(struct brain_struct* brain);

/**
 * @brief Wire world model to active inference system
 *
 * WHAT: Connects world model to active inference for policy evaluation
 * WHY:  Enables active inference to use world model for EFE computation
 * HOW:  Sets up bidirectional callbacks between world model and active inference
 *
 * @param brain Brain structure with initialized world model
 * @return true on success, false on failure
 *
 * @note Requires both world model and active inference to be initialized
 */
bool nimcp_brain_factory_wire_world_model_active_inference(struct brain_struct* brain);

/**
 * @brief Wire world model to imagination engine
 *
 * WHAT: Connects world model to imagination engine for scene generation
 * WHY:  Enables imagination to use world model dynamics for simulation
 * HOW:  Sets up callbacks for world model to provide state transitions
 *
 * @param brain Brain structure with initialized world model
 * @return true on success, false on failure
 *
 * @note Requires both world model and imagination engine to be initialized
 */
bool nimcp_brain_factory_wire_world_model_imagination(struct brain_struct* brain);

/**
 * @brief Wire world model to all integration bridges
 *
 * WHAT: Creates and connects all 11 World Model integration bridges
 * WHY:  Enables bidirectional data flow between WM and brain subsystems
 * HOW:  Creates each bridge based on config flags, connects to brain systems
 *
 * Bridges created (if enabled in config):
 * - Security-Immune Bridge (0x0E63): Anomaly/threat prediction, immune modulation
 * - Logging Bridge (0x0E64): Prediction logging, latent capture, performance
 * - Cognitive Bridge (0x0E65): PFC decisions, goal-directed behavior, planning
 * - Parietal Bridge (0x0E66): Spatial prediction, physics, coordinate transforms
 * - Hypothalamus Bridge (0x0E67): Homeostatic needs, drive prediction
 * - Thalamic Bridge (0x0E68): Sensory gating, attention modulation
 * - Substrate Bridge (0x0E69): Reward/value signals, metabolic context
 * - Memory Bridge (0x0E6A): Hippocampal replay, memory consolidation
 * - KG Bridge (0x0E6B): Knowledge graph entity prediction
 * - ToM Bridge (0x0E6C): Theory of Mind, social trajectory prediction
 * - Plasticity Bridge (0x0E6D): STDP↔RSSM encoder sync, PE feedback
 * - Thousand Brains Bridge (0x0E6E): Hawkins cortical column ref frames, voting, sequences
 *
 * @param brain Brain structure with initialized world model
 * @return true on success, false on failure
 *
 * @note Requires world model to be initialized first
 * @note Non-fatal if individual bridges fail - will continue with others
 */
bool nimcp_brain_factory_wire_world_model_bridges(struct brain_struct* brain);

/**
 * @brief Destroy all world model integration bridges
 *
 * WHAT: Cleans up all WM bridge resources
 * WHY:  Called during brain destruction
 * HOW:  Destroys each bridge in reverse order of creation
 *
 * @param brain Brain structure with initialized bridges
 */
void nimcp_brain_factory_destroy_world_model_bridges(struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_WORLD_MODEL_H
